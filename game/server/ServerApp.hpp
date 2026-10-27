#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "Cli.hpp"
#include "Collisions.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace rtype::server {

class ServerApp {
public:
    ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config,
              const std::filesystem::path& scripts = {});

    bool start(std::string& error);
    void tick(engine::core::time::Duration step);
    void run(const std::function<bool()>& should_stop);
    void stop();

    void apply_inputs(const std::vector<engine::network::PlayerInput>& inputs);
    void replicate();
    void publish_audio();
    void publish_events();

    [[nodiscard]] engine::core::EntityManager& registry() noexcept { return registry_; }
    [[nodiscard]] engine::core::SystemScheduler& systems() noexcept { return systems_; }
    [[nodiscard]] engine::core::event::EventBus& events() noexcept { return events_; }
    [[nodiscard]] engine::network::NetworkServer& network() noexcept { return network_; }
    [[nodiscard]] engine::script::ScriptRuntime& script() noexcept { return _runtime; }
    [[nodiscard]] const engine::network::StringTable& strings() const noexcept { return _strings; }

private:
    std::reference_wrapper<const engine::core::time::IClock> clock_;
    engine::core::event::EventBus events_;
    engine::core::EntityManager registry_;
    engine::core::SystemScheduler systems_;
    engine::script::ScriptRuntime _runtime;
    engine::network::StringTable _strings;
    engine::core::FieldRegistry _fields;
    engine::network::NetworkServer network_;
    Collisions _collisions;
};

} // namespace rtype::server
