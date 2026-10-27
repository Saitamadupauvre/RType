#include "ClientConnection.hpp"

#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"

namespace engine::network {

ClientConnection::ClientConnection(const core::time::IClock& clock,
                                   const core::FieldRegistry& fields, std::string server_label,
                                   std::uint32_t client_nonce, ClientConnectionConfig config)
    : clock_(clock), _server_label(std::move(server_label)), client_nonce_(client_nonce),
      config_(config), _strings(fields), traffic_(clock.now()) {}

std::optional<Outbound> ClientConnection::update() {
    if (_farewell) {
        return std::exchange(_farewell, std::nullopt);
    }
    const auto now = clock_.get().now();
    if (state_ == ConnectionState::connecting) {
        return retry_connect(now);
    }
    if (state_ == ConnectionState::connected) {
        return keep_alive(now);
    }
    return std::nullopt;
}

bool ClientConnection::handle(const Inbound& inbound) {
    if (inbound.from != server_peer) {
        core::log::debug("network: ignored message from a peer other than the server");
        return false;
    }
    if (!is_live()) {
        core::log::debug("network: ignored message from {} after the connection ended",
                         _server_label);
        return false;
    }

    const auto now = clock_.get().now();
    last_received_ = now;
    traffic_.on_received(inbound.message, now);
    const auto& message = inbound.message.message;
    if (!_strings.admits(message)) {
        return false;
    }
    if (std::holds_alternative<HeartbeatPayload>(message)) {
        on_heartbeat_answer(now);
    }
    if (const auto* accept = std::get_if<ConnectAcceptPayload>(&message)) {
        on_accept(*accept);
    } else if (const auto* disconnect = std::get_if<DisconnectPayload>(&message)) {
        on_disconnect(*disconnect);
    }
    return true;
}

std::optional<Outbound> ClientConnection::send_input(const InputPayload& input) {
    if (state_ != ConnectionState::connected) {
        return std::nullopt;
    }
    return to_server(input);
}

std::optional<Outbound> ClientConnection::disconnect() {
    if (!is_live()) {
        return std::nullopt;
    }
    end(ConnectionState::disconnected, DisconnectReason::normal);
    return to_server(DisconnectPayload{.reason = DisconnectReason::normal});
}

bool ClientConnection::is_live() const noexcept {
    return state_ == ConnectionState::connecting || state_ == ConnectionState::connected;
}

std::optional<Outbound> ClientConnection::retry_connect(core::time::Duration now) {
    if (attempts_ > 0 && now - last_sent_ < config_.retry_interval) {
        return std::nullopt;
    }
    if (attempts_ >= config_.max_attempts) {
        end(ConnectionState::gave_up, DisconnectReason::timeout);
        core::log::warn("network: no answer from {} after {} connect attempts", _server_label,
                        attempts_);
        return std::nullopt;
    }

    ++attempts_;
    last_sent_ = now;
    return to_server(ConnectRequestPayload{.client_nonce = client_nonce_});
}

std::optional<Outbound> ClientConnection::keep_alive(core::time::Duration now) {
    if (now - last_received_ > config_.server_timeout) {
        end(ConnectionState::disconnected, DisconnectReason::timeout);
        core::log::warn("network: lost {}: no packet received before the timeout", _server_label);
        return std::nullopt;
    }
    if (now - last_heartbeat_ < config_.heartbeat_interval) {
        return std::nullopt;
    }
    last_heartbeat_ = now;
    unanswered_heartbeat_ = now;
    return to_server(HeartbeatPayload{});
}

void ClientConnection::on_heartbeat_answer(core::time::Duration now) {
    if (unanswered_heartbeat_) {
        traffic_.on_round_trip(now - *unanswered_heartbeat_);
        unanswered_heartbeat_.reset();
    }
}

void ClientConnection::on_accept(const ConnectAcceptPayload& accept) {
    if (state_ != ConnectionState::connecting || accept.client_nonce != client_nonce_) {
        return;
    }
    if (!_strings.assign(accept)) {
        end(ConnectionState::rejected, DisconnectReason::protocol_error);
        _farewell = to_server(DisconnectPayload{.reason = DisconnectReason::protocol_error});
        core::log::error("network: cannot use the fields replicated by {}", _server_label);
        return;
    }
    state_ = ConnectionState::connected;
    last_heartbeat_ = clock_.get().now();
    player_id_ = accept.player_entity_id;
    core::log::info("network: connected to {} as player {}", _server_label, player_id_);
}

void ClientConnection::on_disconnect(const DisconnectPayload& disconnect) {
    if (state_ == ConnectionState::connecting &&
        disconnect.reason == DisconnectReason::server_full) {
        end(ConnectionState::rejected, disconnect.reason);
        core::log::warn("network: {} rejected the connection: server full", _server_label);
        return;
    }
    end(ConnectionState::disconnected, disconnect.reason);
    core::log::info("network: disconnected by {} (reason {})", _server_label,
                    static_cast<int>(disconnect.reason));
}

void ClientConnection::end(ConnectionState state, DisconnectReason reason) {
    state_ = state;
    disconnect_reason_ = reason;
}

Outbound ClientConnection::to_server(Message message) {
    traffic_.on_sent(message, clock_.get().now());
    return Outbound{.to = server_peer, .message = std::move(message), .sequence = next_sequence_++};
}

NetworkStats ClientConnection::stats() const { return traffic_.stats(clock_.get().now()); }

} // namespace engine::network
