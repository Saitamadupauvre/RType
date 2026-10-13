#include "UdpSocket.hpp"

#include "OpGuard.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>

namespace engine::network {

namespace {

using namespace std::chrono_literals;

constexpr auto close_check_interval = 100ms;

#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

std::error_code last_error() noexcept { return {errno, std::generic_category()}; }

std::error_code closed_socket_error() noexcept {
    return std::make_error_code(std::errc::bad_file_descriptor);
}

int open_datagram_socket(int domain) noexcept {
#ifdef SOCK_CLOEXEC
    return ::socket(domain, SOCK_DGRAM | SOCK_CLOEXEC, 0);
#else
    const int fd = ::socket(domain, SOCK_DGRAM, 0);
    if (fd >= 0 && ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        const int saved_errno = errno;
        ::close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
#endif
}

bool is_retryable_receive_error(int error) noexcept {
    return error == EINTR || error == EAGAIN || error == EWOULDBLOCK;
}

} // namespace

std::optional<Endpoint> Endpoint::from_string(std::string_view ip, std::uint16_t port) {
    if (ip.find('\0') != std::string_view::npos) {
        return std::nullopt;
    }

    const std::string address(ip);
    Endpoint endpoint;

    sockaddr_in v4{};
    if (inet_pton(AF_INET, address.c_str(), &v4.sin_addr) == 1) {
        v4.sin_family = AF_INET;
        v4.sin_port = htons(port);
        std::memcpy(&endpoint.storage_, &v4, sizeof(v4));
        endpoint.length_ = sizeof(v4);
        return endpoint;
    }

    sockaddr_in6 v6{};
    if (inet_pton(AF_INET6, address.c_str(), &v6.sin6_addr) == 1) {
        v6.sin6_family = AF_INET6;
        v6.sin6_port = htons(port);
        std::memcpy(&endpoint.storage_, &v6, sizeof(v6));
        endpoint.length_ = sizeof(v6);
        return endpoint;
    }

    return std::nullopt;
}

std::uint16_t Endpoint::port() const noexcept {
    if (storage_.ss_family == AF_INET) {
        sockaddr_in v4{};
        std::memcpy(&v4, &storage_, sizeof(v4));
        return ntohs(v4.sin_port);
    }

    if (storage_.ss_family == AF_INET6) {
        sockaddr_in6 v6{};
        std::memcpy(&v6, &storage_, sizeof(v6));
        return ntohs(v6.sin6_port);
    }

    return 0;
}

std::string Endpoint::to_string() const {
    std::array<char, INET6_ADDRSTRLEN> text{};

    if (storage_.ss_family == AF_INET) {
        sockaddr_in v4{};
        std::memcpy(&v4, &storage_, sizeof(v4));
        inet_ntop(AF_INET, &v4.sin_addr, text.data(), text.size());
        return std::string(text.data()) + ":" + std::to_string(port());
    }

    if (storage_.ss_family == AF_INET6) {
        sockaddr_in6 v6{};
        std::memcpy(&v6, &storage_, sizeof(v6));
        inet_ntop(AF_INET6, &v6.sin6_addr, text.data(), text.size());
        return "[" + std::string(text.data()) + "]:" + std::to_string(port());
    }

    return "<invalid>";
}

UdpSocket::UdpSocket(int domain) {
    const int fd = open_datagram_socket(domain);
    if (fd < 0) {
        throw std::system_error(last_error(), "UdpSocket: socket() failed");
    }
    fd_.store(fd, std::memory_order_release);
}

UdpSocket UdpSocket::create(int domain, std::error_code& ec) noexcept {
    ec.clear();
    UdpSocket socket;
    const int fd = open_datagram_socket(domain);
    if (fd < 0) {
        ec = last_error();
        return socket;
    }
    socket.fd_.store(fd, std::memory_order_release);
    return socket;
}

UdpSocket::~UdpSocket() { close(); }

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : fd_(other.fd_.exchange(no_socket, std::memory_order_acq_rel)) {}

UdpSocket& UdpSocket::operator=(UdpSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_.store(other.fd_.exchange(no_socket, std::memory_order_acq_rel),
                  std::memory_order_release);
    }
    return *this;
}

void UdpSocket::close() noexcept {
    const int fd = fd_.exchange(no_socket, std::memory_order_seq_cst);
    if (fd == no_socket) {
        return;
    }

    ::shutdown(fd, SHUT_RDWR);

    for (unsigned int pending = active_ops_.load(std::memory_order_seq_cst); pending != 0;
         pending = active_ops_.load(std::memory_order_seq_cst)) {
        active_ops_.wait(pending, std::memory_order_seq_cst);
    }

    ::close(fd);
}

void UdpSocket::bind(const Endpoint& local, std::error_code& ec) noexcept {
    ec.clear();
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return;
    }

    if (::bind(guard.fd, local.addr(), local.size()) < 0) {
        ec = last_error();
    }
}

void UdpSocket::set_reuse_address(bool enable, std::error_code& ec) noexcept {
    ec.clear();
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return;
    }

    const int value = enable ? 1 : 0;
    if (::setsockopt(guard.fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) {
        ec = last_error();
    }
}

Endpoint UdpSocket::local_endpoint(std::error_code& ec) const noexcept {
    ec.clear();
    Endpoint local;
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return local;
    }

    local.prepare_for_receive();
    socklen_t length = local.size();
    if (::getsockname(guard.fd, local.addr(), &length) < 0) {
        ec = last_error();
        return {};
    }
    local.set_size(length);
    return local;
}

std::size_t UdpSocket::send_to(std::span<const std::uint8_t> data, const Endpoint& to,
                               std::error_code& ec) noexcept {
    ec.clear();
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return 0;
    }

    for (;;) {
        const ssize_t sent =
            ::sendto(guard.fd, data.data(), data.size(), send_flags, to.addr(), to.size());
        if (sent >= 0) {
            return static_cast<std::size_t>(sent);
        }
        if (errno != EINTR) {
            ec = last_error();
            return 0;
        }
    }
}

std::size_t UdpSocket::receive_from(std::span<std::uint8_t> buffer, Endpoint& from,
                                    std::chrono::milliseconds timeout,
                                    std::error_code& ec) noexcept {
    using clock = std::chrono::steady_clock;

    ec.clear();
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return 0;
    }

    const bool waits_forever = timeout.count() < 0;
    const auto deadline = clock::now() + std::max(timeout, 0ms);

    for (;;) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now());
        const auto wait =
            waits_forever ? close_check_interval : std::clamp(remaining, 0ms, close_check_interval);

        pollfd request{.fd = guard.fd, .events = POLLIN, .revents = 0};
        const int ready = ::poll(&request, 1, static_cast<int>(wait.count()));
        if (ready < 0 && errno != EINTR) {
            ec = last_error();
            return 0;
        }

        if (guard.closing()) {
            ec = std::make_error_code(std::errc::operation_canceled);
            return 0;
        }

        if (ready > 0) {
            from.prepare_for_receive();
            socklen_t length = from.size();
            const ssize_t received = ::recvfrom(guard.fd, buffer.data(), buffer.size(),
                                                MSG_DONTWAIT, from.addr(), &length);
            if (received >= 0) {
                from.set_size(length);
                return static_cast<std::size_t>(received);
            }
            if (!is_retryable_receive_error(errno)) {
                ec = last_error();
                return 0;
            }
        }

        if (!waits_forever && clock::now() >= deadline) {
            ec = std::make_error_code(std::errc::timed_out);
            return 0;
        }
    }
}

} // namespace engine::network
