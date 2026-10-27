#include "NetworkIo.hpp"

#include <array>
#include <chrono>
#include <utility>

#include "engine/core/log/Log.hpp"

#include "protocol/Protocol.hpp"
#include "socket/SocketError.hpp"
#include "thread/DatagramHandler.hpp"

namespace engine::network {

namespace {

using namespace std::chrono_literals;

constexpr auto wait_forever = -1ms;
constexpr std::size_t oversize_detecting_buffer_size = max_datagram_size + 1;

bool is_shutdown_error(const std::error_code& ec) {
    return ec == SocketError::canceled || ec == SocketError::closed;
}

} // namespace

NetworkIo::NetworkIo(std::size_t inbox_capacity) : inbox_(inbox_capacity) {}

NetworkIo::~NetworkIo() { stop(); }

void NetworkIo::start(const Endpoint& bind_to, std::error_code& ec) {
    ec.clear();
    if (socket_.is_open()) {
        ec = std::make_error_code(std::errc::already_connected);
        return;
    }

    auto socket = UdpSocket::create(bind_to.addr()->sa_family, ec);
    if (ec) {
        return;
    }
    socket.bind(bind_to, ec);
    if (ec) {
        return;
    }

    socket_ = std::move(socket);
    receive_thread_ =
        std::jthread([this](const std::stop_token& stop_token) { run_receive(stop_token); });
    send_thread_ =
        std::jthread([this](const std::stop_token& stop_token) { run_send(stop_token); });
}

void NetworkIo::stop() {
    send_thread_.request_stop();
    if (send_thread_.joinable()) {
        send_thread_.join();
    }
    socket_.close();
    receive_thread_.request_stop();
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
}

std::vector<AddressedInbound> NetworkIo::poll() { return inbox_.drain(); }

void NetworkIo::send(AddressedOutbound outbound) { outbox_.push(std::move(outbound)); }

Endpoint NetworkIo::local_endpoint(std::error_code& ec) const { return socket_.local_endpoint(ec); }

void NetworkIo::run_receive(const std::stop_token& stop_token) {
    std::array<std::uint8_t, oversize_detecting_buffer_size> buffer{};
    while (!stop_token.stop_requested() && receive_one(buffer)) {
    }
}

void NetworkIo::run_send(const std::stop_token& stop_token) {
    while (!stop_token.stop_requested()) {
        send_all(outbox_.wait_and_drain(stop_token));
    }
    send_all(outbox_.drain());
}

bool NetworkIo::receive_one(std::span<std::uint8_t> buffer) {
    Endpoint from;
    std::error_code ec;
    const auto size = socket_.receive_from(buffer, from, wait_forever, ec);
    if (is_shutdown_error(ec)) {
        return false;
    }
    if (ec) {
        core::log::warn("network: receive failed: {}", ec.message());
        return true;
    }

    if (size > max_datagram_size) {
        core::log::warn("network: dropped oversized datagram from {}", from.to_string());
        return true;
    }

    auto inbound = handle_datagram(protocol_, buffer.first(size), from);
    if (inbound && !inbox_.push(std::move(*inbound))) {
        core::log::warn("network: inbox full, dropped message from {}", from.to_string());
    }
    return true;
}

void NetworkIo::send_all(const std::vector<AddressedOutbound>& outbound) {
    for (const auto& message : outbound) {
        const auto bytes = encode_outbound(protocol_, message, message.sequence);
        if (!bytes) {
            continue;
        }

        std::error_code ec;
        socket_.send_to(*bytes, message.to, ec);
        if (ec) {
            core::log::warn("network: failed to send {} bytes to {}: {}", bytes->size(),
                            message.to.to_string(), ec.message());
        }
    }
}

} // namespace engine::network
