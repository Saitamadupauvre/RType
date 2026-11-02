#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"
#include "protocol/ReceivedStrings.hpp"
#include "stats/TrafficMeter.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

struct ClientConnectionConfig {
    core::time::Duration retry_interval = std::chrono::milliseconds(500);
    std::size_t max_attempts = 5;
    core::time::Duration heartbeat_interval = std::chrono::seconds(1);
    core::time::Duration server_timeout = std::chrono::seconds(5);
};

class ENGINE_NETWORK_EXPORT ClientConnection {
public:
    ClientConnection(const core::time::IClock& clock, const core::FieldRegistry& fields,
                     std::string server_label, std::uint32_t client_nonce,
                     ClientConnectionConfig config = {});

    [[nodiscard]] std::optional<Outbound> update();
    bool handle(const Inbound& inbound);
    [[nodiscard]] std::optional<Outbound> send_input(const InputPayload& input);
    [[nodiscard]] std::optional<Outbound> disconnect();

    [[nodiscard]] ConnectionState state() const noexcept { return state_; }
    [[nodiscard]] std::optional<DisconnectReason> disconnect_reason() const noexcept {
        return disconnect_reason_;
    }
    [[nodiscard]] std::uint32_t player_id() const noexcept { return player_id_; }
    [[nodiscard]] const ReceivedStrings& strings() const noexcept { return _strings; }
    [[nodiscard]] NetworkStats stats() const;

private:
    [[nodiscard]] bool is_live() const noexcept;
    [[nodiscard]] std::optional<Outbound> retry_connect(core::time::Duration now);
    [[nodiscard]] std::optional<Outbound> keep_alive(core::time::Duration now);
    void on_heartbeat_answer(core::time::Duration now);
    void on_accept(const ConnectAcceptPayload& accept);
    void on_disconnect(const DisconnectPayload& disconnect);
    void end(ConnectionState state, DisconnectReason reason);
    [[nodiscard]] Outbound to_server(Message message);

    std::reference_wrapper<const core::time::IClock> clock_;
    std::string _server_label;
    std::uint32_t client_nonce_;
    ClientConnectionConfig config_;
    ConnectionState state_{ConnectionState::connecting};
    std::optional<DisconnectReason> disconnect_reason_;
    std::size_t attempts_{0};
    core::time::Duration last_sent_{};
    core::time::Duration last_received_{};
    core::time::Duration last_heartbeat_{};
    std::optional<core::time::Duration> unanswered_heartbeat_;
    std::uint32_t next_sequence_{0};
    std::uint32_t player_id_{0};
    ReceivedStrings _strings;
    std::optional<Outbound> _farewell;
    TrafficMeter traffic_;
};

} // namespace engine::network
