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
        return keep_alive(now);
    }
    return std::nullopt;
}

bool ClientConnection::handle(const Inbound& inbound) {
    if (inbound.from != server_) {
        core::log::debug("network: ignored message from {}, not the server",
                         inbound.from.to_string());
        return false;
    }
    if (!is_live()) {
        core::log::debug("network: ignored message from {} after the connection ended",
                         server_.to_string());
        return false;
    }

    last_received_ = clock_.get().now();
    const auto& message = inbound.message.message;
    if (!type_table_.admits(message)) {
        return false;
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
    last_sent_ = clock_.get().now();
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
        core::log::warn("network: no answer from {} after {} connect attempts", server_.to_string(),
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
        core::log::warn("network: lost {}: no packet received before the timeout",
                        server_.to_string());
        return std::nullopt;
    }
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
    type_table_.assign(accept);
    core::log::info("network: connected to {} as player {}", server_.to_string(), player_id_);
}

void ClientConnection::on_disconnect(const DisconnectPayload& disconnect) {
    if (state_ == ConnectionState::connecting &&
        disconnect.reason == DisconnectReason::server_full) {
        end(ConnectionState::rejected, disconnect.reason);
        core::log::warn("network: {} rejected the connection: server full", server_.to_string());
        return;
    }
    end(ConnectionState::disconnected, disconnect.reason);
    core::log::info("network: disconnected by {} (reason {})", server_.to_string(),
                    static_cast<int>(disconnect.reason));
}

void ClientConnection::end(ConnectionState state, DisconnectReason reason) {
    state_ = state;
    disconnect_reason_ = reason;
}

Outbound ClientConnection::to_server(Message message) const {
    return Outbound{.to = server_, .message = std::move(message)};
}

} // namespace engine::network
