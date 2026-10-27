#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Stats.hpp"

#include "session/ServerSessions.hpp"
#include "snapshot/ReplicatedFields.hpp"
#include "stats/TrafficMeter.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT ServerNetwork {
public:
    ServerNetwork(const core::time::IClock& clock, core::event::EventBus& events,
                  const core::FieldRegistry& fields, ServerSessionsConfig config);

    [[nodiscard]] std::vector<Outbound> receive(const std::vector<Inbound>& inbound);
    void queue_event(const EventPayload& event);
    void set_current_music(std::uint16_t music_id) { sessions_.set_current_music(music_id); }
    [[nodiscard]] std::vector<Outbound> snapshot(core::EntityManager& world);
    [[nodiscard]] std::vector<Outbound> shutdown();

    [[nodiscard]] const std::vector<Session>& sessions() const noexcept {
        return sessions_.sessions();
    }
    [[nodiscard]] std::uint32_t server_tick() const noexcept { return server_tick_; }
    [[nodiscard]] const StringTable& strings() const noexcept { return sessions_.strings(); }
    [[nodiscard]] const ReplicatedFields& replicated() const noexcept { return _replicated; }
    [[nodiscard]] std::vector<PlayerStats> stats() const { return sessions_.stats(); }

private:
    void report_stats();

    std::reference_wrapper<const core::time::IClock> clock_;
    core::time::Duration last_report_;
    ReplicatedFields _replicated;
    ServerSessions sessions_;
    std::vector<Outbound> queued_events_;
    std::uint32_t server_tick_{0};
};

} // namespace engine::network
