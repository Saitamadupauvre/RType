#include "ServerSessions.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <variant>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "protocol/Sequence.hpp"

namespace engine::network {

ServerSessions::ServerSessions(const core::time::IClock& clock, core::event::EventBus& events,
                               ServerSessionsConfig config)
    : clock_(clock), events_(events), config_(std::move(config)) {}

std::vector<Outbound> ServerSessions::handle(const Inbound& inbound) {
    const auto& message = inbound.message.message;
    const auto* request = std::get_if<ConnectRequestPayload>(&message);
    const auto session = std::ranges::find(sessions_, inbound.from, &Session::endpoint);

    if (session == sessions_.end()) {
        if (request != nullptr) {
            return admit(inbound.from, *request);
        }
        core::log::debug("network: ignored message from unknown endpoint {}",
                         inbound.from.to_string());
        return {};
    }

    session->last_seen = clock_.get().now();
    if (request != nullptr) {
        return {accept_message(*session, *request)};
    }
    if (const auto* input = std::get_if<InputPayload>(&message)) {
        store_input(*session, *input, inbound.message.header.sequence);
        return {};
    }
    if (std::holds_alternative<DisconnectPayload>(message)) {
        const auto player_id = session->player_id;
        core::log::info("network: player {} at {} disconnected", player_id,
                        session->endpoint.to_string());
        sessions_.erase(session);
        return leave(player_id);
    }
    return {};
}

std::vector<Outbound> ServerSessions::drop_timed_out() {
    const auto now = clock_.get().now();
    const auto timed_out = [this, now](const Session& session) {
        return now - session.last_seen > config_.timeout;
    };

    std::vector<Session> expired;
    std::ranges::copy_if(sessions_, std::back_inserter(expired), timed_out);
    std::erase_if(sessions_, timed_out);

    std::vector<Outbound> outbound;
    for (const auto& session : expired) {
        core::log::info("network: player {} at {} timed out", session.player_id,
                        session.endpoint.to_string());
        outbound.push_back(
            Outbound{.to = session.endpoint,
                     .message = DisconnectPayload{.reason = DisconnectReason::timeout}});
        std::ranges::move(leave(session.player_id), std::back_inserter(outbound));
    }
    return outbound;
}

std::vector<Outbound> ServerSessions::admit(const Endpoint& from,
                                            const ConnectRequestPayload& request) {
    if (sessions_.size() >= config_.max_players) {
        core::log::info("network: rejected {}: server full", from.to_string());
        return {Outbound{.to = from,
                         .message = DisconnectPayload{.reason = DisconnectReason::server_full}}};
    }

    const auto& session = sessions_.emplace_back(
        Session{.endpoint = from, .player_id = free_player_id(), .last_seen = clock_.get().now()});
    core::log::info("network: player {} joined from {}", session.player_id, from.to_string());
    events_.get().publish(PlayerJoined{.player_id = session.player_id});
    return {accept_message(session, request)};
}

std::vector<Outbound> ServerSessions::broadcast(const Message& message) const {
    std::vector<Outbound> outbound;
    outbound.reserve(sessions_.size());
    for (const auto& session : sessions_) {
        outbound.push_back(Outbound{.to = session.endpoint, .message = message});
    }
    return outbound;
}

std::vector<Outbound> ServerSessions::leave(std::uint32_t player_id) {
    events_.get().publish(PlayerLeft{.player_id = player_id});
    return broadcast(EventPayload{PlayerLeftEvent{.player_entity_id = player_id}});
}

void ServerSessions::store_input(Session& session, const InputPayload& input,
                                 std::uint32_t sequence) {
    if (session.input_sequence && !is_newer_sequence(sequence, *session.input_sequence)) {
        core::log::debug("network: dropped stale input {} from player {}", sequence,
                         session.player_id);
        return;
    }
    session.input = input;
    session.input_sequence = sequence;
}

Outbound ServerSessions::accept_message(const Session& session,
                                        const ConnectRequestPayload& request) const {
    return Outbound{.to = session.endpoint,
                    .message = ConnectAcceptPayload{.client_nonce = request.client_nonce,
                                                    .player_entity_id = session.player_id,
                                                    .types = config_.types}};
}

std::uint32_t ServerSessions::free_player_id() const {
    std::uint32_t player_id = 1;
    while (std::ranges::find(sessions_, player_id, &Session::player_id) != sessions_.end()) {
        ++player_id;
    }
    return player_id;
}

} // namespace engine::network
