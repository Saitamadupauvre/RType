#include "engine/app/ServerApp.hpp"

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "Collisions.hpp"
#include "Fields.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"

namespace engine::app {

namespace {

using namespace std::chrono_literals;

constexpr auto idle_between_frames = 1ms;

template <typename Component> void register_if_missing(core::EntityManager& world) {
    if (!world.is_registered<Component>()) {
        world.register_component<Component>();
    }
}

script::ScriptRuntime load_game(core::EntityManager& world, const std::filesystem::path& scripts) {
    auto runtime = script::ScriptRuntime::builder().with<script::ServerApi>().build();
    runtime.attach_world(world);
    if (!scripts.empty()) {
        runtime.load_scripts(scripts);
    }
    return runtime;
}

network::StringTable strings_of(const script::ScriptRuntime& runtime) {
    network::StringTable strings;
    const auto add_all = [&strings](network::StringCategory category,
                                    std::vector<std::string> names, std::string_view kind) {
        for (std::string& name : names) {
            if (!strings.add(category, name)) {
                core::log::warn("server: {} '{}' cannot be sent to clients", kind, name);
            }
        }
    };
    add_all(network::StringCategory::type, runtime.entity_types(), "entity type");
    add_all(network::StringCategory::sound, runtime.sound_names(), "sound");
    add_all(network::StringCategory::music, runtime.music_names(), "music");
    add_all(network::StringCategory::event, runtime.event_names(), "event");
    add_all(network::StringCategory::event_key, runtime.event_keys(), "event key");
    return strings;
}

static_assert(script::max_event_arguments <= network::max_event_arguments);
static_assert(script::max_event_text_length <= network::max_event_text_length);

network::NamedEvent to_named_event(script::EventCommand command) {
    network::NamedEvent event{.name = std::move(command.name), .arguments = {}};
    event.arguments.reserve(command.arguments.size());
    for (auto& argument : command.arguments) {
        event.arguments.push_back(
            {.key = std::move(argument.key), .value = std::move(argument.value)});
    }
    return event;
}

std::uint32_t wire_source(std::optional<core::Entity> source) {
    return source ? source->index() : network::no_source_entity;
}

std::uint8_t wire_owner(std::optional<std::uint32_t> owner) {
    if (!owner || *owner > std::numeric_limits<std::uint8_t>::max()) {
        return 0;
    }
    return static_cast<std::uint8_t>(*owner);
}

} // namespace

struct ServerApp::Impl {
    core::event::EventBus events;
    core::EntityManager world;
    core::SystemScheduler systems;
    script::ScriptRuntime runtime;
    script::ServerApi& server;
    network::StringTable strings;
    core::FieldRegistry fields;
    network::NetworkServer network;
    Collisions collisions;
    core::time::FixedStepLoop loop;

    Impl(const core::time::IClock& clock, network::ServerTransport& transport,
         const ServerAppOptions& options);

    void tick(core::time::Duration step);
    void apply_inputs();
    void replicate();
    void publish_audio();
    void publish_events();
};

ServerApp::Impl::Impl(const core::time::IClock& clock, network::ServerTransport& transport,
                      const ServerAppOptions& options)
    : runtime(load_game(world, options.scripts)), server(runtime.api<script::ServerApi>()),
      strings(strings_of(runtime)), fields(replicated_fields()),
      network(clock, events, fields, transport,
              network::ServerOptions{.max_players = options.max_players, .strings = strings}),
      collisions(world, events, runtime), loop(clock) {
    register_if_missing<network::Replicated>(world);
    events.subscribe<network::PlayerJoined>(
        [this](const network::PlayerJoined& joined) { server.player_joined(joined.player_id); });
    events.subscribe<network::PlayerLeft>(
        [this](const network::PlayerLeft& left) { server.player_left(left.player_id); });
}

void ServerApp::Impl::tick(core::time::Duration step) {
    const float dt = std::chrono::duration<float>(step).count();
    network.receive();
    apply_inputs();
    runtime.update(dt);
    collisions.step(dt);
    runtime.flush_destroyed();
    systems.run(world, dt);
    replicate();
    publish_audio();
    publish_events();
    network.send_snapshot(world);
}

void ServerApp::Impl::apply_inputs() {
    for (const auto& input : network.inputs()) {
        server.set_player_input(input.player_id, input.input);
    }
}

void ServerApp::Impl::replicate() {
    std::vector<std::pair<core::Entity, network::Replicated>> updates;
    world.query<core::Position, core::Sprite>().each([&](core::Entity entity, const core::Position&,
                                                         const core::Sprite&) {
        const auto type = runtime.entity_type(entity);
        if (!type) {
            return;
        }
        const auto type_id = strings.id_of(network::StringCategory::type, *type);
        if (!type_id) {
            return;
        }
        updates.emplace_back(entity,
                             network::Replicated{.type_id = *type_id,
                                                 .owner = wire_owner(server.entity_owner(entity))});
    });
    for (const auto& [entity, replicated] : updates) {
        if (world.has<network::Replicated>(entity)) {
            world.get<network::Replicated>(entity) = replicated;
        } else {
            world.add(entity, replicated);
        }
    }
}

void ServerApp::Impl::publish_audio() {
    for (const auto& command : server.take_audio_commands()) {
        if (command.kind == script::AudioCommandKind::StopMusic) {
            events.publish(network::MusicChanged{.music_id = network::no_music});
            continue;
        }
        const bool is_sound = command.kind == script::AudioCommandKind::Sound;
        const auto id = strings.id_of(is_sound ? network::StringCategory::sound
                                               : network::StringCategory::music,
                                      command.name);
        if (!id) {
            continue;
        }
        if (is_sound) {
            events.publish(network::SoundPlayed{.source_entity_id = wire_source(command.source),
                                                .sound_id = *id});
        } else {
            events.publish(network::MusicChanged{.music_id = *id});
        }
    }
}

void ServerApp::Impl::publish_events() {
    for (auto& command : server.take_event_commands()) {
        events.publish(to_named_event(std::move(command)));
    }
}

ServerApp::ServerApp(const core::time::IClock& clock, network::ServerTransport& transport,
                     const ServerAppOptions& options)
    : _impl(std::make_unique<Impl>(clock, transport, options)) {}

ServerApp::~ServerApp() = default;

bool ServerApp::start(std::error_code& ec) { return _impl->network.start(ec); }

void ServerApp::tick(core::time::Duration step) { _impl->tick(step); }

void ServerApp::update() {
    _impl->loop.run_frame([this](core::time::Duration step) { _impl->tick(step); });
}

void ServerApp::run(const std::function<bool()>& should_stop) {
    while (!should_stop()) {
        update();
        std::this_thread::sleep_for(idle_between_frames);
    }
    stop();
}

void ServerApp::stop() { _impl->network.stop(); }

core::EntityManager& ServerApp::world() noexcept { return _impl->world; }

core::SystemScheduler& ServerApp::systems() noexcept { return _impl->systems; }

core::event::EventBus& ServerApp::events() noexcept { return _impl->events; }

network::NetworkServer& ServerApp::network() noexcept { return _impl->network; }

script::ScriptRuntime& ServerApp::script() noexcept { return _impl->runtime; }

script::ServerApi& ServerApp::script_server() noexcept { return _impl->server; }

const network::StringTable& ServerApp::strings() const noexcept { return _impl->strings; }

} // namespace engine::app
