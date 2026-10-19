#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

enum class ConnectionState : std::uint8_t {
    connecting,
    connected,
    rejected,
    gave_up,
    disconnected,
};

struct ClientConnectionConfig {
    core::time::Duration retry_interval = std::chrono::milliseconds(500);
    std::size_t max_attempts = 5;
    core::time::Duration heartbeat_interval = std::chrono::seconds(1);
};

class ENGINE_NETWORK_EXPORT ClientConnection {
public:
    ClientConnection(const core::time::IClock& clock, const Endpoint& server,
                     std::uint32_t client_nonce, ClientConnectionConfig config = {});

    [[nodiscard]] std::optional<Outbound> update();
    void handle(const Inbound& inbound);
    [[nodiscard]] std::optional<Outbound> disconnect();

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] std::uint32_t player_id() const noexcept { return player_id_; }
    [[nodiscard]] const std::vector<TypeEntry>& types() const noexcept { return types_; }

private:
    [[nodiscard]] std::optional<Outbound> retry_connect(core::time::Duration now);
    [[nodiscard]] std::optional<Outbound> send_heartbeat(core::time::Duration now);
    void on_accept(const ConnectAcceptPayload& accept);
    void on_disconnect(const DisconnectPayload& disconnect);
    [[nodiscard]] Outbound to_server(Message message) const;

    std::reference_wrapper<const core::time::IClock> clock_;
    Endpoint server_;
    std::uint32_t client_nonce_;
    ClientConnectionConfig config_;
    ConnectionState state_{ConnectionState::connecting};
    std::size_t attempts_{0};
    core::time::Duration last_sent_{};
    std::uint32_t player_id_{0};
    std::vector<TypeEntry> types_;
};

} // namespace engine::network
