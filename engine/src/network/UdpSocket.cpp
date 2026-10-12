#include "UdpSocket.hpp"
#include "OpGuard.hpp"

#include <arpa/inet.h>
#include <cstddef>
#include <poll.h>
#include <system_error>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

namespace engine::network {

namespace {

std::error_code lastError() noexcept { return {errno, std::generic_category()}; }

} // namespace

std::optional<Endpoint> Endpoint::fromString(std::string_view ip, std::uint16_t port) {

    const std::string s(ip); // null-terminated copy for inet_pton
    Endpoint e;

    sockaddr_in v4{};
    if (inet_pton(AF_INET, s.c_str(), &v4.sin_addr) == 1) {
        v4.sin_family = AF_INET;
        v4.sin_port   = htons(port);
        std::memcpy(&e._ss, &v4, sizeof(v4));
        e._len = sizeof(v4);
        return e;
    }

    sockaddr_in6 v6{};
    if (inet_pton(AF_INET6, s.c_str(), &v6.sin6_addr) == 1) {
        v6.sin6_family = AF_INET6;
        v6.sin6_port   = htons(port);
        std::memcpy(&e._ss, &v6, sizeof(v6));
        e._len = sizeof(v6);
        return e;
    }

    return std::nullopt;
}

std::uint16_t Endpoint::port() const noexcept {

    if (_ss.ss_family == AF_INET) {
        sockaddr_in v4;
        std::memcpy(&v4, &_ss, sizeof(v4));
        return ntohs(v4.sin_port);
    }

    if (_ss.ss_family == AF_INET6) {
        sockaddr_in6 v6;
        std::memcpy(&v6, &_ss, sizeof(v6));
        return ntohs(v6.sin6_port);
    }

    return 0;
}

std::string Endpoint::toString() const {

    char buf[INET6_ADDRSTRLEN] = {}; // Avoids casting back char* for inet_ntop
    if (_ss.ss_family == AF_INET) {
        sockaddr_in v4;
        std::memcpy(&v4, &_ss, sizeof(v4));
        inet_ntop(AF_INET, &v4.sin_addr, buf, sizeof(buf));
        return std::string(buf) + ":" + std::to_string(port());
    }

    if (_ss.ss_family == AF_INET6) {
        sockaddr_in6 v6;
        std::memcpy(&v6, &_ss, sizeof(v6));
        inet_ntop(AF_INET6, &v6.sin6_addr, buf, sizeof(buf));
        return "[" + std::string(buf) + "]:" + std::to_string(port());
    }

    return "<invalid>";
}

UdpSocket::UdpSocket(int domain, int type) : _fd(EMPTY_SOCKET) {

    const int fd = ::socket(domain, type | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        throw std::system_error(lastError(), "UdpSocket: socket() failed");
    }

    _fd.store(fd, std::memory_order_release);
}

UdpSocket UdpSocket::create(int domain, std::error_code& ec, int type) noexcept {

    ec.clear();
    UdpSocket s;
    const int fd = ::socket(domain, type | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        ec = lastError();
        return s;
    }
    s._fd.store(fd, std::memory_order_release);

    return s;
}

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : _fd(other._fd.exchange(EMPTY_SOCKET, std::memory_order_acq_rel)) {}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {

    if (this != &other) {
        close();
        _fd.store(other._fd.exchange(EMPTY_SOCKET, std::memory_order_acq_rel),
                  std::memory_order_release);
    }

    return *this;
}

void UdpSocket::close() noexcept {

    // Only one caller wins the exchange and performs the real close.
    const int fd = _fd.exchange(EMPTY_SOCKET, std::memory_order_seq_cst);
    if (fd == EMPTY_SOCKET) {
        return;
    }

    ::shutdown(fd, SHUT_RDWR); // best effort: wake threads blocked in poll/recv (ENOTCONN is fine)

    // Wait for in-flight operations to leave; they see the fd change and bail out.
    for (unsigned n = _activeOps.load(std::memory_order_seq_cst); n != 0;
         n = _activeOps.load(std::memory_order_seq_cst))
        _activeOps.wait(n, std::memory_order_seq_cst);

    ::close(fd); // do not retry on EINTR: on Linux the fd is released regardless
}

void UdpSocket::bind(const Endpoint& local, std::error_code& ec) noexcept {

    ec.clear();
    detail::OpGuard g(_fd, _activeOps);
    if (!g.valid()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    if (::bind(g.fd, local.addr(), local.size()) < 0) {
        ec = lastError();
    }
}

void UdpSocket::setReuseAddress(bool enable, std::error_code& ec) noexcept {

    ec.clear();
    detail::OpGuard g(_fd, _activeOps);
    if (!g.valid()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return;
    }

    const int v = enable ? 1 : 0;
    if (::setsockopt(g.fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) < 0) {
        ec = lastError();
    }
}

std::size_t UdpSocket::sendTo(std::span<const std::byte> data, const Endpoint& to,
                              std::error_code& ec) noexcept {
    ec.clear();
    detail::OpGuard g(_fd, _activeOps);
    if (!g.valid()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    for (;;) {
        const ssize_t n = ::sendto(g.fd, data.data(), data.size(), MSG_NOSIGNAL, to.addr(), to.size());
        if (n >= 0) {
            return static_cast<std::size_t>(n);
        }

        if (errno == EINTR) {
            continue;
        }

        ec = lastError();
        return 0;
    }
}

std::size_t UdpSocket::receiveFrom(std::span<std::byte> buf, Endpoint& from,
                                   std::chrono::milliseconds timeout, std::error_code& ec) noexcept {
    using clock = std::chrono::steady_clock;
    constexpr auto kSlice = std::chrono::milliseconds(100); // upper bound to notice close()

    ec.clear();
    detail::OpGuard g(_fd, _activeOps);
    if (!g.valid()) {
        ec = std::make_error_code(std::errc::bad_file_descriptor);
        return 0;
    }

    const bool infinite = timeout.count() < 0;
    const auto deadline = clock::now() + (infinite ? std::chrono::milliseconds(0) : timeout);

    for (;;) {
        if (g.closing()) { ec = std::make_error_code(std::errc::operation_canceled); return 0; }

        auto wait = kSlice;
        if (!infinite) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now());
            if (left.count() <= 0 && timeout.count() != 0) {
                ec = std::make_error_code(std::errc::timed_out);
                return 0;
            }
            wait = std::min(kSlice, std::max(left, std::chrono::milliseconds(0)));
        }

        pollfd p{g.fd, POLLIN, 0};
        const int pr = ::poll(&p, 1, static_cast<int>(wait.count()));
        if (pr < 0) {
            if (errno == EINTR) continue;
            ec = lastError();
            return 0;
        }

        if (pr == 0) {
            if (!infinite && clock::now() >= deadline) {
                ec = std::make_error_code(std::errc::timed_out);
                return 0;
            }
            continue;
        }

        if (g.closing()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return 0;
        }

        if (p.revents & (POLLERR | POLLNVAL)) {
            // POLLERR on UDP: pending async error (e.g. ICMP); recvfrom below will report it.
        }

        from.prepareForReceive();
        socklen_t len = from.size();
        // MSG_DONTWAIT: another thread may have taken the datagram between poll and recv.
        const ssize_t n = ::recvfrom(g.fd, buf.data(), buf.size(), MSG_DONTWAIT, from.addr(), &len);
        if (n >= 0) {
            from.setSize(len);
            return static_cast<std::size_t>(n);
        }

        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            continue;
        }
        ec = lastError();
        return 0;
    }
}

} // namespace engine::network
