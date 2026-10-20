#include "ServerApp.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/script/InputState.hpp"

namespace rtype::server {

namespace {

using namespace std::chrono_literals;

constexpr auto idle_between_frames = 1ms;
constexpr std::uint16_t player_type_id = 1;
constexpr std::string_view player_entity_type = "square";
constexpr std::uint8_t fire_flag = 1U;
constexpr float axis_range = 127.F;
constexpr float spawn_x = 100.F;
constexpr float first_spawn_y = 120.F;
constexpr float spawn_spacing = 140.F;

float to_axis(std::int8_t value) {
    return std::clamp(static_cast<float>(value) / axis_range, -1.F, 1.F);
}

engine::script::InputState to_input_state(const engine::network::PlayerInput& input) {
    engine::script::InputState state;
    state.axes.emplace("move_x", to_axis(input.move_x));
    state.axes.emplace("move_y", to_axis(input.move_y));
    if ((input.action_flags & fire_flag) != 0) {
        state.pressed.emplace("fire");
    }
    return state;
}

engine::core::Vec2 spawn_position(std::uint32_t player_id) {
    const auto slot = static_cast<float>(player_id % max_supported_players);
    return {spawn_x, first_spawn_y + (slot * spawn_spacing)};
}

template <typename Component> void register_if_missing(engine::core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

} // namespace

ServerApp::ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config,
                     const std::filesystem::path& scripts)
    : clock_(clock),
      network_(
          clock, events_,
          engine::network::ServerOptions{
              .port = config.port,
              .max_players = config.max_players,
              .types = {{.type_id = player_type_id, .name = std::string(player_entity_type)}}}) {
    register_if_missing<engine::network::Replicated>(registry_);
    register_if_missing<engine::core::Position>(registry_);
    register_if_missing<engine::core::Velocity>(registry_);
    _runtime.attach_world(registry_);
    if (!scripts.empty()) {
        _runtime.load_scripts(scripts);
    }
    events_.subscribe<engine::network::PlayerJoined>(
        [this](const engine::network::PlayerJoined& joined) { spawn_player(joined.player_id); });
    events_.subscribe<engine::network::PlayerLeft>(
        [this](const engine::network::PlayerLeft& left) { remove_player(left.player_id); });
}

bool ServerApp::start(std::string& error) {
    std::error_code ec;
    if (!network_.start(ec)) {
        error = "cannot listen on port " + std::to_string(network_.port()) + ": " + ec.message();
        return false;
    }
    return true;
}

void ServerApp::tick(engine::core::time::Duration step) {
    const float dt = std::chrono::duration<float>(step).count();
    network_.receive();
    apply_inputs(network_.inputs());
    _runtime.update(dt);
    _runtime.flush_destroyed();
    systems_.run(registry_, dt);
    network_.send_snapshot(registry_);
}

void ServerApp::apply_inputs(const std::vector<engine::network::PlayerInput>& inputs) {
    for (const auto& input : inputs) {
        const auto player = _players.find(input.player_id);
        if (player != _players.end()) {
            _runtime.set_input(player->second, to_input_state(input));
        }
    }
}

std::optional<engine::core::Entity> ServerApp::player_entity(std::uint32_t player_id) const {
    const auto player = _players.find(player_id);
    if (player == _players.end()) {
        return std::nullopt;
    }
    return player->second;
}

void ServerApp::spawn_player(std::uint32_t player_id) {
    if (_players.contains(player_id)) {
        return;
    }
    const auto entity = _runtime.spawn(player_entity_type, spawn_position(player_id));
    if (!entity) {
        return;
    }
    registry_.add(*entity, engine::network::Replicated{.type_id = player_type_id});
    _players.emplace(player_id, *entity);
}

void ServerApp::remove_player(std::uint32_t player_id) {
    const auto player = _players.find(player_id);
    if (player == _players.end()) {
        return;
    }
    _runtime.release(player->second);
    registry_.kill(player->second);
    _players.erase(player);
}

void ServerApp::run(const std::function<bool()>& should_stop) {
    engine::core::time::FixedStepLoop loop(clock_.get());
    while (!should_stop()) {
        loop.run_frame([this](engine::core::time::Duration step) { tick(step); });
        std::this_thread::sleep_for(idle_between_frames);
    }
    stop();
}

void ServerApp::stop() { network_.stop(); }

} // namespace rtype::server
