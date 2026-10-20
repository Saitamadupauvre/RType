#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Cli.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Server.hpp"
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
    [[nodiscard]] std::optional<engine::core::Entity> player_entity(std::uint32_t player_id) const;

    [[nodiscard]] engine::core::EntityManager& registry() noexcept { return registry_; }
    [[nodiscard]] engine::core::SystemScheduler& systems() noexcept { return systems_; }
    [[nodiscard]] engine::core::event::EventBus& events() noexcept { return events_; }
    [[nodiscard]] engine::network::NetworkServer& network() noexcept { return network_; }

private:
    void spawn_player(std::uint32_t player_id);
    void remove_player(std::uint32_t player_id);

    std::reference_wrapper<const engine::core::time::IClock> clock_;
    engine::core::event::EventBus events_;
    engine::core::EntityManager registry_;
    engine::core::SystemScheduler systems_;
    engine::network::NetworkServer network_;
    engine::script::ScriptRuntime _runtime;
    std::unordered_map<std::uint32_t, engine::core::Entity> _players;
};

} // namespace rtype::server
