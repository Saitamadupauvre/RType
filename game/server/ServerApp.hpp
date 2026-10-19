#pragma once

#include <functional>
#include <string>

#include "Cli.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Server.hpp"

namespace rtype::server {

class ServerApp {
public:
    ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config);

    bool start(std::string& error);
    void tick(engine::core::time::Duration step);
    void run(const std::function<bool()>& should_stop);
    void stop();

    [[nodiscard]] engine::core::EntityManager& registry() noexcept { return registry_; }
    [[nodiscard]] engine::core::SystemScheduler& systems() noexcept { return systems_; }
    [[nodiscard]] engine::core::event::EventBus& events() noexcept { return events_; }
    [[nodiscard]] engine::network::NetworkServer& network() noexcept { return network_; }

private:
    std::reference_wrapper<const engine::core::time::IClock> clock_;
    engine::core::event::EventBus events_;
    engine::core::EntityManager registry_;
    engine::core::SystemScheduler systems_;
    engine::network::NetworkServer network_;
};

} // namespace rtype::server
