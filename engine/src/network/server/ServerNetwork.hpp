#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"

#include "session/ServerSessions.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT ServerNetwork {
public:
    ServerNetwork(const core::time::IClock& clock, core::event::EventBus& events,
                  ServerSessionsConfig config);

    [[nodiscard]] std::vector<Outbound> receive(const std::vector<Inbound>& inbound);
    void queue_event(const EventPayload& event);
    [[nodiscard]] std::vector<Outbound> snapshot(core::EntityManager& registry);
    [[nodiscard]] std::vector<Outbound> shutdown();

    [[nodiscard]] const std::vector<Session>& sessions() const noexcept {
        return sessions_.sessions();
    }
    [[nodiscard]] std::uint32_t server_tick() const noexcept { return server_tick_; }

private:
    ServerSessions sessions_;
    std::vector<Outbound> queued_events_;
    std::uint32_t server_tick_{0};
};

} // namespace engine::network
