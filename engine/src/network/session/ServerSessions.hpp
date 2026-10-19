#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"

#include "protocol/Messages.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

struct ServerSessionsConfig {
    std::size_t max_players{};
    core::time::Duration timeout = std::chrono::seconds(5);
    std::vector<TypeEntry> types{};
};

struct Session {
    Endpoint endpoint;
    std::uint32_t player_id{};
    core::time::Duration last_seen{};
    InputPayload input{};
    std::optional<std::uint32_t> input_sequence{};
};

class ENGINE_NETWORK_EXPORT ServerSessions {
public:
    ServerSessions(const core::time::IClock& clock, core::event::EventBus& events,
                   ServerSessionsConfig config);

    [[nodiscard]] std::vector<Outbound> handle(const Inbound& inbound);
    [[nodiscard]] std::vector<Outbound> drop_timed_out();
    [[nodiscard]] std::vector<Outbound> broadcast(const Message& message) const;

    [[nodiscard]] const std::vector<Session>& sessions() const noexcept { return sessions_; }

private:
    [[nodiscard]] std::vector<Outbound> admit(const Endpoint& from,
                                              const ConnectRequestPayload& request);
    [[nodiscard]] std::vector<Outbound> leave(std::uint32_t player_id);
    static void store_input(Session& session, const InputPayload& input, std::uint32_t sequence);
    [[nodiscard]] Outbound accept_message(const Session& session,
                                          const ConnectRequestPayload& request) const;
    [[nodiscard]] std::uint32_t free_player_id() const;

    std::reference_wrapper<const core::time::IClock> clock_;
    std::reference_wrapper<core::event::EventBus> events_;
    ServerSessionsConfig config_;
    std::vector<Session> sessions_;
};

} // namespace engine::network
