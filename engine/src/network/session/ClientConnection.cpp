#include "ClientConnection.hpp"

#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"

namespace engine::network {

ClientConnection::ClientConnection(const core::time::IClock& clock, const Endpoint& server,
                                   std::uint32_t client_nonce, ClientConnectionConfig config)
    : clock_(clock), server_(server), client_nonce_(client_nonce), config_(config) {}

std::optional<Outbound> ClientConnection::update() {
    const auto now = clock_.get().now();
    if (state_ == ConnectionState::connecting) {
        return retry_connect(now);
    }
    if (state_ == ConnectionState::connected) {
        return send_heartbeat(now);
    }
    return std::nullopt;
}

void ClientConnection::handle(const Inbound& inbound) {
    if (inbound.from != server_) {
        core::log::debug("network: ignored message from {}, not the server",
                         inbound.from.to_string());
        return;
    }

    const auto& message = inbound.message.message;
    if (const auto* accept = std::get_if<ConnectAcceptPayload>(&message)) {
        on_accept(*accept);
    } else if (const auto* disconnect = std::get_if<DisconnectPayload>(&message)) {
        on_disconnect(*disconnect);
    }
}

std::optional<Outbound> ClientConnection::disconnect() {
    if (state_ != ConnectionState::connecting && state_ != ConnectionState::connected) {
        return std::nullopt;
    }
    state_ = ConnectionState::disconnected;
    return to_server(DisconnectPayload{.reason = DisconnectReason::normal});
}

std::optional<Outbound> ClientConnection::retry_connect(core::time::Duration now) {
    if (attempts_ > 0 && now - last_sent_ < config_.retry_interval) {
        return std::nullopt;
    }
    if (attempts_ >= config_.max_attempts) {
        state_ = ConnectionState::gave_up;
        core::log::warn("network: no answer from {} after {} connect attempts", server_.to_string(),
                        attempts_);
        return std::nullopt;
    }

    ++attempts_;
    last_sent_ = now;
    return to_server(ConnectRequestPayload{.client_nonce = client_nonce_});
}

std::optional<Outbound> ClientConnection::send_heartbeat(core::time::Duration now) {
    if (now - last_sent_ < config_.heartbeat_interval) {
        return std::nullopt;
    }
    last_sent_ = now;
    return to_server(HeartbeatPayload{});
}

void ClientConnection::on_accept(const ConnectAcceptPayload& accept) {
    if (state_ != ConnectionState::connecting || accept.client_nonce != client_nonce_) {
        return;
    }
    state_ = ConnectionState::connected;
    player_id_ = accept.player_entity_id;
    types_ = accept.types;
    core::log::info("network: connected to {} as player {}", server_.to_string(), player_id_);
}

void ClientConnection::on_disconnect(const DisconnectPayload& disconnect) {
    if (state_ == ConnectionState::connecting &&
        disconnect.reason == DisconnectReason::server_full) {
        state_ = ConnectionState::rejected;
        core::log::warn("network: {} rejected the connection: server full", server_.to_string());
        return;
    }
    if (state_ == ConnectionState::connecting || state_ == ConnectionState::connected) {
        state_ = ConnectionState::disconnected;
        core::log::info("network: disconnected by {}", server_.to_string());
    }
}

Outbound ClientConnection::to_server(Message message) const {
    return Outbound{.to = server_, .message = std::move(message)};
}

} // namespace engine::network
