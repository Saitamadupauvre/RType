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

#include "Fields.hpp"
#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/metrics/ProcessUsage.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "simulation/CameraFollow.hpp"
#include "simulation/PhysicsStep.hpp"

namespace engine::app {

namespace {

using namespace std::chrono_literals;

constexpr auto idle_between_frames = 1ms;

template <typename Component> void register_if_missing(core::EntityManager& world) {
    if (!world.is_registered<Component>()) {
        world.register_component<Component>();
    }
}

script::ScriptRuntime load_game(core::EntityManager& world, const std::filesystem::path& game) {
    auto runtime = script::ScriptRuntime::builder().with<script::ServerApi>().build();
    runtime.attach_world(world);
    if (!game.empty()) {
        runtime.load_game(game);
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
    add_all(network::StringCategory::texture, runtime.texture_names(), "texture");
    add_all(network::StringCategory::shader, runtime.shader_names(), "shader");
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
    PhysicsStep physics;
    core::time::FixedStepLoop loop;
    core::metrics::Metrics metrics;
    core::metrics::ProcessMonitor process;

    Impl(const core::time::IClock& clock, network::ServerTransport& transport,
         const ServerAppOptions& options);

    void tick(core::time::Duration step);
    void simulate(float dt);
    void publish_gauges();
    void apply_inputs();
    void replicate();
    void publish_audio();
    void publish_events();
};

ServerApp::Impl::Impl(const core::time::IClock& clock, network::ServerTransport& transport,
                      const ServerAppOptions& options)
    : runtime(load_game(world, options.game)), server(runtime.api<script::ServerApi>()),
      strings(strings_of(runtime)), fields(replicated_fields()),
      network(clock, events, fields, transport,
              network::ServerOptions{.max_players = options.max_players, .strings = strings}),
      physics(world, events, runtime), loop(clock), metrics(clock), process(metrics) {
    register_if_missing<network::Replicated>(world);
    events.subscribe<network::PlayerJoined>(
        [this](const network::PlayerJoined& joined) { server.player_joined(joined.player_id); });
    events.subscribe<network::PlayerLeft>(
        [this](const network::PlayerLeft& left) { server.player_left(left.player_id); });
}

void ServerApp::Impl::tick(core::time::Duration step) {
    {
        const core::metrics::ScopedTimer timer(metrics, "tick");
        simulate(std::chrono::duration<float>(step).count());
    }
    publish_gauges();
    metrics.end_frame();
}

void ServerApp::Impl::simulate(float dt) {
    {
        const core::metrics::ScopedTimer timer(metrics, "network.receive");
        network.receive();
        apply_inputs();
    }
    {
        const core::metrics::ScopedTimer timer(metrics, "lua.update");
        runtime.update(dt);
    }
    {
        const core::metrics::ScopedTimer timer(metrics, "physics");
        physics.step(dt);
        runtime.flush_destroyed();
    }
    runtime.flush_scene();
    {
        const core::metrics::ScopedTimer timer(metrics, "systems");
        systems.run(world, dt);
        follow_cameras(world);
    }
    const core::metrics::ScopedTimer timer(metrics, "network.send");
    replicate();
    publish_audio();
    publish_events();
    network.send_snapshot(world);
}

void ServerApp::Impl::publish_gauges() {
    if (!metrics.enabled()) {
        return;
    }
    metrics.set_gauge("entities", static_cast<double>(world.size()));
    metrics.set_gauge("players", static_cast<double>(network.player_count()));
    metrics.set_gauge("snapshot.bytes", static_cast<double>(network.last_snapshot_bytes()));
    process.update();
}

void ServerApp::Impl::apply_inputs() {
    for (const auto& input : network.inputs()) {
        server.set_player_input(input.player_id, input.input);
    }
}

void ServerApp::Impl::replicate() {
    std::vector<std::pair<core::Entity, network::Replicated>> updates;
    const auto mark = [&](core::Entity entity) {
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
    };
    world.query<core::Position, core::Sprite>().each(
        [&](core::Entity entity, const core::Position&, const core::Sprite&) { mark(entity); });
    world.query<core::Position, core::Camera>().each(
        [&](core::Entity entity, const core::Position&, const core::Camera&) {
            if (!world.has<core::Sprite>(entity)) {
                mark(entity);
            }
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

core::metrics::Metrics& ServerApp::metrics() noexcept { return _impl->metrics; }

} // namespace engine::app
