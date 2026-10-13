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

#include "engine/network/Export.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT Endpoint {
public:
    Endpoint() = default;

    static std::optional<Endpoint> from_string(std::string_view ip, std::uint16_t port);

    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] const sockaddr* addr() const noexcept {
        return reinterpret_cast<const sockaddr*>(&storage_);
    }
    sockaddr* addr() noexcept { return reinterpret_cast<sockaddr*>(&storage_); }
    [[nodiscard]] socklen_t size() const noexcept { return length_; }

    void prepare_for_receive() noexcept { length_ = sizeof(storage_); }
    void set_size(socklen_t length) noexcept { length_ = length; }

private:
    sockaddr_storage storage_{};
    socklen_t length_{0};
};

class ENGINE_NETWORK_EXPORT UdpSocket {
public:
    UdpSocket() noexcept = default;
    explicit UdpSocket(int domain);
    static UdpSocket create(int domain, std::error_code& ec) noexcept;

    ~UdpSocket();

    UdpSocket(UdpSocket&& other) noexcept;
    UdpSocket& operator=(UdpSocket&& other) noexcept;

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    [[nodiscard]] bool is_open() const noexcept {
        return fd_.load(std::memory_order_acquire) != no_socket;
    }
    explicit operator bool() const noexcept { return is_open(); }

    [[nodiscard]] int native_handle() const noexcept { return fd_.load(std::memory_order_acquire); }

    void close() noexcept;

    void bind(const Endpoint& local, std::error_code& ec) noexcept;
    void set_reuse_address(bool enable, std::error_code& ec) noexcept;
    [[nodiscard]] Endpoint local_endpoint(std::error_code& ec) const noexcept;

    std::size_t send_to(std::span<const std::uint8_t> data, const Endpoint& to,
                        std::error_code& ec) noexcept;

    std::size_t receive_from(std::span<std::uint8_t> buffer, Endpoint& from,
                             std::chrono::milliseconds timeout, std::error_code& ec) noexcept;

private:
    static constexpr int no_socket = -1;

    std::atomic<int> fd_{no_socket};
    mutable std::atomic<unsigned int> active_ops_{0};
};

} // namespace engine::network
