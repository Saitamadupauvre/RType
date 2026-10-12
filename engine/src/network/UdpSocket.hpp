#pragma once

#include <netinet/in.h>
#include <sys/socket.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

constexpr int EMPTY_SOCKET = -1;

namespace engine::network {

/// Value type holding an IPv4/IPv6 address + port. Trivially copyable, thread-safe to read.
class Endpoint {
public:
    Endpoint() = default;

    /// Parses "1.2.3.4" or "::1". Returns nullopt on malformed input.
    static std::optional<Endpoint> fromString(std::string_view ip, std::uint16_t port);

    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] std::string toString() const;

    [[nodiscard]] const sockaddr* addr() const noexcept {
        return reinterpret_cast<const sockaddr*>(&_ss);
    }
    sockaddr* addr() noexcept { return reinterpret_cast<sockaddr*>(&_ss); }
    [[nodiscard]] socklen_t size() const noexcept { return _len; }

    /// Reset to the full capacity, to be passed to recvfrom().
    void prepareForReceive() noexcept { _len = sizeof(_ss); }
    void setSize(socklen_t len) noexcept { _len = len; }

private:
    sockaddr_storage _ss{};
    socklen_t _len{0};
};

/// RAII, move-only UDP socket.
///
/// Thread-safety:
///  - bind / sendTo / receiveFrom / close / isOpen may be called concurrently from any threads
///    (including close() while another thread is blocked in receiveFrom()).
///  - Move construction / move assignment / destruction require exclusive access
///    (nobody else may be using the object), as with any C++ object.
///
/// Errors are reported through std::error_code (no exceptions except the constructor).
class UdpSocket {

public:
    UdpSocket() noexcept : _fd(EMPTY_SOCKET) {}

    /// Creates the socket (SOCK_CLOEXEC is always added). Throws std::system_error on failure.
    explicit UdpSocket(int domain, int type = SOCK_DGRAM);

    /// Non-throwing factory.
    static UdpSocket create(int domain, std::error_code& ec, int type = SOCK_DGRAM) noexcept;

    ~UdpSocket();

    UdpSocket(UdpSocket&&) noexcept;
    UdpSocket& operator=(UdpSocket&&) noexcept;

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    [[nodiscard]] bool isOpen() const noexcept {
        return _fd.load(std::memory_order_acquire) != EMPTY_SOCKET;
    }
    explicit operator bool() const noexcept { return isOpen(); }

    /// Raw fd for read-only inspection. Do not close it; it may become invalid if close() races.
    [[nodiscard]] int nativeHandle() const noexcept { return _fd.load(std::memory_order_acquire); }

    /// Idempotent. Wakes up blocked receivers (they get std::errc::operation_canceled),
    /// waits for in-flight calls to finish, then closes the fd.
    void close() noexcept;

    void bind(const Endpoint& local, std::error_code& ec) noexcept;
    void setReuseAddress(bool enable, std::error_code& ec) noexcept;

    /// Sends one datagram. Returns bytes sent.
    std::size_t sendTo(std::span<const std::byte> data, const Endpoint& to,
                       std::error_code& ec) noexcept;

    /// Receives one datagram. A negative timeout waits indefinitely.
    /// On timeout, ec == std::errc::timed_out. Datagrams larger than buf are truncated.
    std::size_t receiveFrom(std::span<std::byte> buf, Endpoint& from,
                            std::chrono::milliseconds timeout, std::error_code& ec) noexcept;

private:
    class OpGuard;

    std::atomic<int> _fd;
    std::atomic<unsigned int> _activeOps{0}; // calls currently using the fd
};

} // namespace engine::network
