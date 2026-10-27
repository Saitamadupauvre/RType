#include "ServerApp.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

#include "GameFields.hpp"
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

template <typename Component> void register_if_missing(engine::core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

engine::network::StringTable load_game(engine::script::ScriptRuntime& runtime,
                                       engine::core::EntityManager& registry,
                                       const std::filesystem::path& scripts) {
    runtime.attach_world(registry);
    if (!scripts.empty()) {
        runtime.load_scripts(scripts);
    }
    engine::network::StringTable strings;
    const auto add_all = [&strings](engine::network::StringCategory category,
                                    std::vector<std::string> names, std::string_view kind) {
        for (std::string& name : names) {
            if (!strings.add(category, name)) {
                engine::core::log::warn("server: {} '{}' cannot be sent to clients", kind, name);
            }
        }
    };
    add_all(engine::network::StringCategory::type, runtime.entity_types(), "entity type");
    add_all(engine::network::StringCategory::sound, runtime.sound_names(), "sound");
    add_all(engine::network::StringCategory::music, runtime.music_names(), "music");
    add_all(engine::network::StringCategory::event, runtime.event_names(), "event");
    add_all(engine::network::StringCategory::event_key, runtime.event_keys(), "event key");
    return strings;
}

static_assert(engine::script::max_event_arguments <= engine::network::max_event_arguments);
static_assert(engine::script::max_event_text_length <= engine::network::max_event_text_length);

engine::network::NamedEvent to_named_event(engine::script::EventCommand command) {
    engine::network::NamedEvent event{.name = std::move(command.name), .arguments = {}};
    event.arguments.reserve(command.arguments.size());
    for (auto& argument : command.arguments) {
        event.arguments.push_back(
            {.key = std::move(argument.key), .value = std::move(argument.value)});
    }
    return event;
}

std::uint32_t wire_source(std::optional<engine::core::Entity> source) {
    return source ? source->index() : engine::network::no_source_entity;
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
    : clock_(clock),
      _runtime(engine::script::ScriptRuntime::builder().with<engine::script::ServerApi>().build()),
      _server(_runtime.api<engine::script::ServerApi>()),
      _strings(load_game(_runtime, registry_, scripts)), _fields(common::game_fields()),
      network_(clock, events_, _fields,
               engine::network::ServerOptions{
                   .port = config.port, .max_players = config.max_players, .strings = _strings}),
      _collisions(registry_, events_, _runtime) {
    register_if_missing<engine::network::Replicated>(registry_);
    register_if_missing<engine::core::Position>(registry_);
    register_if_missing<engine::core::Sprite>(registry_);
    events_.subscribe<engine::network::PlayerJoined>(
        [this](const engine::network::PlayerJoined& joined) {
            _server.player_joined(joined.player_id);
        });
    events_.subscribe<engine::network::PlayerLeft>(
        [this](const engine::network::PlayerLeft& left) { _server.player_left(left.player_id); });
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
    _collisions.step(dt);
    _runtime.flush_destroyed();
    systems_.run(registry_, dt);
    replicate();
    publish_audio();
    publish_events();
    network_.send_snapshot(registry_);
}

void ServerApp::publish_audio() {
    for (const auto& command : _server.take_audio_commands()) {
        if (command.kind == engine::script::AudioCommandKind::StopMusic) {
            events_.publish(engine::network::MusicChanged{.music_id = engine::network::no_music});
            continue;
        }
        const bool is_sound = command.kind == engine::script::AudioCommandKind::Sound;
        const auto id = _strings.id_of(is_sound ? engine::network::StringCategory::sound
                                                : engine::network::StringCategory::music,
                                       command.name);
        if (!id) {
            continue;
        }
        if (is_sound) {
            events_.publish(engine::network::SoundPlayed{
                .source_entity_id = wire_source(command.source), .sound_id = *id});
        } else {
            events_.publish(engine::network::MusicChanged{.music_id = *id});
        }
    }
}

void ServerApp::publish_events() {
    for (auto& command : _server.take_event_commands()) {
        events_.publish(to_named_event(std::move(command)));
    }
}

void ServerApp::apply_inputs(const std::vector<engine::network::PlayerInput>& inputs) {
    for (const auto& input : inputs) {
        _server.set_player_input(input.player_id, input.input);
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
            const auto type_id = _strings.id_of(engine::network::StringCategory::type, *type);
            if (!type_id) {
                return;
            }
            updates.emplace_back(entity, engine::network::Replicated{
                                             .type_id = *type_id,
                                             .owner = wire_owner(_server.entity_owner(entity))});
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
