#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/MessageQueue.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

inline constexpr std::size_t default_inbox_capacity = 4096;

class ENGINE_NETWORK_EXPORT NetworkIo : core::NonCopyableNonMovable {
public:
    explicit NetworkIo(std::size_t inbox_capacity = default_inbox_capacity);
    ~NetworkIo();

    void start(const Endpoint& bind_to, std::error_code& ec);
    void stop();

    [[nodiscard]] std::vector<Inbound> poll();
    void send(Outbound outbound);

    [[nodiscard]] Endpoint local_endpoint(std::error_code& ec) const;

private:
    void run_receive(const std::stop_token& stop_token);
    void run_send(const std::stop_token& stop_token);
    bool receive_one(std::span<std::uint8_t> buffer);
    void send_all(const std::vector<Outbound>& outbound);

    UdpSocket socket_;
    ProtocolHandler protocol_;
    MessageQueue<Inbound> inbox_;
    MessageQueue<Outbound> outbox_;
    std::jthread receive_thread_;
    std::jthread send_thread_;
};

} // namespace engine::network
