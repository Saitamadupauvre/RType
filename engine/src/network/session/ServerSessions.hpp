#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Stats.hpp"

#include "protocol/Messages.hpp"
#include "stats/TrafficMeter.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::string describe_peer_id(PeerId peer);

struct ServerSessionsConfig {
    std::size_t max_players{};
    core::time::Duration timeout = std::chrono::seconds(5);
    StringTable strings{};
    std::uint16_t current_music{no_music};
    std::function<std::string(PeerId)> describe_peer = describe_peer_id;
};

struct Session {
    PeerId peer{};
    std::uint32_t player_id{};
    core::time::Duration last_seen{};
    InputPayload input{};
    std::optional<std::uint32_t> input_sequence{};
    std::uint32_t next_sequence{};
    TrafficMeter traffic{};
};

class ENGINE_NETWORK_EXPORT ServerSessions {
public:
    ServerSessions(const core::time::IClock& clock, core::event::EventBus& events,
                   ServerSessionsConfig config);

    [[nodiscard]] std::vector<Outbound> handle(const Inbound& inbound);
    [[nodiscard]] std::vector<Outbound> drop_timed_out();
    [[nodiscard]] std::vector<Outbound> broadcast(const Message& message);
    [[nodiscard]] std::vector<Outbound> close_all(DisconnectReason reason);
    void set_current_music(std::uint16_t music_id);

    [[nodiscard]] const std::vector<Session>& sessions() const noexcept { return sessions_; }
    [[nodiscard]] const StringTable& strings() const noexcept { return config_.strings; }
    [[nodiscard]] std::vector<PlayerStats> stats() const;

private:
    [[nodiscard]] std::vector<Outbound> admit(const Inbound& inbound,
                                              const ConnectRequestPayload& request);
    [[nodiscard]] std::vector<Outbound> leave(std::uint32_t player_id);
    static void store_input(Session& session, const InputPayload& input, std::uint32_t sequence);
    [[nodiscard]] Outbound send_to(Session& session, Message message) const;
    [[nodiscard]] Outbound accept_message(Session& session,
                                          const ConnectRequestPayload& request) const;
    [[nodiscard]] std::uint32_t free_player_id() const;
    [[nodiscard]] std::string describe(PeerId peer) const;

    std::reference_wrapper<const core::time::IClock> clock_;
    std::reference_wrapper<core::event::EventBus> events_;
    ServerSessionsConfig config_;
    std::vector<Session> sessions_;
};

} // namespace engine::network
