#include "UdpSocket.hpp"

#include "OpGuard.hpp"
#include "SocketError.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>

namespace engine::network {

namespace {

using namespace std::chrono_literals;

constexpr auto close_check_interval = 100ms;

#if defined(_WIN32)
using io_result_t = int;
using poll_fd_t = WSAPOLLFD;
constexpr int shutdown_both = SD_BOTH;
constexpr int send_flags = 0;
constexpr int recv_flags = 0;

void ensure_winsock_initialized() noexcept {
    static const struct WinsockInit {
        WinsockInit() noexcept {
            WSADATA data{};
            WSAStartup(MAKEWORD(2, 2), &data);
        }
        ~WinsockInit() { WSACleanup(); }
        WinsockInit(const WinsockInit&) = delete;
        WinsockInit& operator=(const WinsockInit&) = delete;
        WinsockInit(WinsockInit&&) = delete;
        WinsockInit& operator=(WinsockInit&&) = delete;
    } winsock_init;
    (void)winsock_init;
}

int last_socket_errno() noexcept { return WSAGetLastError(); }

int to_fd(SOCKET socket) noexcept {
    return socket == INVALID_SOCKET ? -1 : static_cast<int>(socket);
}

SOCKET to_native(int fd) noexcept {
    return fd < 0 ? INVALID_SOCKET : static_cast<SOCKET>(static_cast<UINT_PTR>(fd));
}

int poll_fds(poll_fd_t* fds, unsigned long count, int timeout) noexcept {
    return WSAPoll(fds, count, timeout);
}

void close_native(int fd) noexcept { ::closesocket(to_native(fd)); }

#else
using io_result_t = ssize_t;
using poll_fd_t = pollfd;
constexpr int shutdown_both = SHUT_RDWR;

#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

#ifdef MSG_DONTWAIT
constexpr int recv_flags = MSG_DONTWAIT;
#else
constexpr int recv_flags = 0;
#endif

void ensure_winsock_initialized() noexcept {}

int last_socket_errno() noexcept { return errno; }

int to_native(int fd) noexcept { return fd; }

int poll_fds(poll_fd_t* fds, nfds_t count, int timeout) noexcept {
    return ::poll(fds, count, timeout);
}

void close_native(int fd) noexcept { ::close(fd); }

#endif

#if defined(_WIN32)
int socket_errno_of(int wsa_error) noexcept {
    switch (wsa_error) {
    case WSAEBADF:
    case WSAENOTSOCK:
        return EBADF;
    case WSAEADDRINUSE:
        return EADDRINUSE;
    case WSAEADDRNOTAVAIL:
        return EADDRNOTAVAIL;
    case WSAEACCES:
        return EACCES;
    case WSAECONNREFUSED:
        return ECONNREFUSED;
    case WSAEMSGSIZE:
        return EMSGSIZE;
    default:
        return wsa_error;
    }
}
#else
int socket_errno_of(int error) noexcept { return error; }
#endif

std::error_code last_error() noexcept {
    return socket_error_from_errno(socket_errno_of(last_socket_errno()));
}

std::error_code closed_socket_error() noexcept { return make_error_code(SocketError::closed); }

int open_datagram_socket(int domain) noexcept {
    ensure_winsock_initialized();
#if defined(_WIN32)
    return to_fd(::socket(domain, SOCK_DGRAM, 0));
#elif defined(SOCK_CLOEXEC)
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
#if defined(_WIN32)
    return error == WSAEINTR || error == WSAEWOULDBLOCK;
#else
    return error == EINTR || error == EAGAIN || error == EWOULDBLOCK;
#endif
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

bool Endpoint::operator==(const Endpoint& other) const noexcept {
    if (storage_.ss_family != other.storage_.ss_family) {
        return false;
    }

    if (storage_.ss_family == AF_INET) {
        sockaddr_in lhs{};
        sockaddr_in rhs{};
        std::memcpy(&lhs, &storage_, sizeof(lhs));
        std::memcpy(&rhs, &other.storage_, sizeof(rhs));
        return lhs.sin_port == rhs.sin_port && lhs.sin_addr.s_addr == rhs.sin_addr.s_addr;
    }

    if (storage_.ss_family == AF_INET6) {
        sockaddr_in6 lhs{};
        sockaddr_in6 rhs{};
        std::memcpy(&lhs, &storage_, sizeof(lhs));
        std::memcpy(&rhs, &other.storage_, sizeof(rhs));
        return lhs.sin6_port == rhs.sin6_port && lhs.sin6_scope_id == rhs.sin6_scope_id &&
               std::memcmp(&lhs.sin6_addr, &rhs.sin6_addr, sizeof(lhs.sin6_addr)) == 0;
    }

    return true;
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

    ::shutdown(to_native(fd), shutdown_both);

    for (unsigned int pending = active_ops_.load(std::memory_order_seq_cst); pending != 0;
         pending = active_ops_.load(std::memory_order_seq_cst)) {
        active_ops_.wait(pending, std::memory_order_seq_cst);
    }

    close_native(fd);
}

void UdpSocket::bind(const Endpoint& local, std::error_code& ec) noexcept {
    ec.clear();
    const detail::OpGuard guard(fd_, active_ops_);
    if (!guard.valid()) {
        ec = closed_socket_error();
        return;
    }

    if (::bind(to_native(guard.fd), local.addr(), local.size()) < 0) {
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
    if (::setsockopt(to_native(guard.fd), SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&value), sizeof(value)) < 0) {
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
    if (::getsockname(to_native(guard.fd), local.addr(), &length) < 0) {
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
        const io_result_t sent =
            ::sendto(to_native(guard.fd), reinterpret_cast<const char*>(data.data()),
                     static_cast<int>(data.size()), send_flags, to.addr(), to.size());
        if (sent >= 0) {
            return static_cast<std::size_t>(sent);
        }
#if defined(_WIN32)
        if (last_socket_errno() != WSAEINTR) {
#else
        if (errno != EINTR) {
#endif
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

        poll_fd_t request{.fd = to_native(guard.fd), .events = POLLIN, .revents = 0};
        const int ready = poll_fds(&request, 1, static_cast<int>(wait.count()));
#if defined(_WIN32)
        if (ready < 0 && last_socket_errno() != WSAEINTR) {
#else
        if (ready < 0 && errno != EINTR) {
#endif
            ec = last_error();
            return 0;
        }

        if (guard.closing()) {
            ec = make_error_code(SocketError::canceled);
            return 0;
        }

        if (ready > 0) {
            from.prepare_for_receive();
            socklen_t length = from.size();
            const io_result_t received =
                ::recvfrom(to_native(guard.fd), reinterpret_cast<char*>(buffer.data()),
                           static_cast<int>(buffer.size()), recv_flags, from.addr(), &length);
            if (received >= 0) {
                from.set_size(length);
                return static_cast<std::size_t>(received);
            }
            if (!is_retryable_receive_error(last_socket_errno())) {
                ec = last_error();
                return 0;
            }
        }

        if (!waits_forever && clock::now() >= deadline) {
            ec = make_error_code(SocketError::timed_out);
            return 0;
        }
    }
}

} // namespace engine::network
