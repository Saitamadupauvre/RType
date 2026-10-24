#include "ServerApp.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

#include "ActionCodec.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/network/Events.hpp"

namespace rtype::server {

namespace {

using namespace std::chrono_literals;

constexpr auto idle_between_frames = 1ms;
constexpr std::size_t max_type_name_length = 63;

template <typename Component> void register_if_missing(engine::core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

std::vector<engine::network::ReplicatedType> load_game(engine::script::ScriptRuntime& runtime,
                                                       engine::core::EntityManager& registry,
                                                       const std::filesystem::path& scripts) {
    runtime.attach_world(registry);
    if (!scripts.empty()) {
        runtime.load_scripts(scripts);
    }
    std::vector<engine::network::ReplicatedType> types;
    for (std::string& name : runtime.entity_types()) {
        if (name.size() > max_type_name_length ||
            types.size() == std::numeric_limits<std::uint16_t>::max()) {
            engine::core::log::warn("server: entity type '{}' cannot be replicated", name);
            continue;
        }
        const auto type_id = static_cast<std::uint16_t>(types.size() + 1);
        types.push_back({.type_id = type_id, .name = std::move(name)});
    }
    return types;
}

std::unordered_map<std::string, std::uint16_t>
index_types(const std::vector<engine::network::ReplicatedType>& types) {
    std::unordered_map<std::string, std::uint16_t> ids;
    for (const auto& type : types) {
        ids.emplace(type.name, type.type_id);
    }
    return ids;
}

std::uint8_t wire_owner(std::optional<std::uint32_t> owner) {
    if (!owner || *owner > std::numeric_limits<std::uint8_t>::max()) {
        return 0;
    }
    return static_cast<std::uint8_t>(*owner);
}

} // namespace

ServerApp::ServerApp(const engine::core::time::IClock& clock, const ServerConfig& config,
                     const std::filesystem::path& scripts)
    : clock_(clock), _types(load_game(_runtime, registry_, scripts)),
      _type_ids(index_types(_types)),
      network_(clock, events_,
               engine::network::ServerOptions{
                   .port = config.port, .max_players = config.max_players, .types = _types}) {
    register_if_missing<engine::network::Replicated>(registry_);
    register_if_missing<engine::core::Position>(registry_);
    register_if_missing<engine::core::Velocity>(registry_);
    register_if_missing<engine::core::Sprite>(registry_);
    events_.subscribe<engine::network::PlayerJoined>(
        [this](const engine::network::PlayerJoined& joined) {
            _runtime.player_joined(joined.player_id);
        });
    events_.subscribe<engine::network::PlayerLeft>(
        [this](const engine::network::PlayerLeft& left) { _runtime.player_left(left.player_id); });
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
    replicate();
    network_.send_snapshot(registry_);
}

void ServerApp::apply_inputs(const std::vector<engine::network::PlayerInput>& inputs) {
    for (const auto& input : inputs) {
        _runtime.set_player_input(input.player_id,
                                  common::unpack_input({.action_flags = input.action_flags,
                                                        .move_x = input.move_x,
                                                        .move_y = input.move_y}));
    }
}

void ServerApp::replicate() {
    std::vector<std::pair<engine::core::Entity, engine::network::Replicated>> updates;
    registry_.query<engine::core::Position, engine::core::Sprite>().each(
        [&](engine::core::Entity entity, const engine::core::Position&,
            const engine::core::Sprite&) {
            const auto type = _runtime.entity_type(entity);
            if (!type) {
                return;
            }
            const auto type_id = _type_ids.find(*type);
            if (type_id == _type_ids.end()) {
                return;
            }
            updates.emplace_back(entity, engine::network::Replicated{
                                             .type_id = type_id->second,
                                             .owner = wire_owner(_runtime.entity_owner(entity))});
        });
    for (const auto& [entity, replicated] : updates) {
        if (registry_.has<engine::network::Replicated>(entity)) {
            registry_.get<engine::network::Replicated>(entity) = replicated;
        } else {
            registry_.add(entity, replicated);
        }
    }
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
