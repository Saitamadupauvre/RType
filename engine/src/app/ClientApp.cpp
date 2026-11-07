#include "engine/app/ClientApp.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Fields.hpp"
#include "assets/AssetPreloader.hpp"
#include "assets/AudioPlayer.hpp"
#include "assets/ShaderLibrary.hpp"
#include "assets/TextureLibrary.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/metrics/ProcessUsage.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/graphics/InputCapture.hpp"
#include "engine/network/Events.hpp"
#include "engine/script/EventCommand.hpp"
#include "render/CameraRig.hpp"
#include "render/HudDraw.hpp"
#include "render/LoadingScreen.hpp"
#include "render/SnapshotView.hpp"
#include "render/WorldDraw.hpp"
#include "simulation/PhysicsStep.hpp"

namespace engine::app {

namespace {

script::ScriptRuntime load_game(core::EntityManager& local, const std::filesystem::path& scripts) {
    auto runtime = script::ScriptRuntime::builder().with<script::ClientApi>().build();
    runtime.attach_world(local);
    if (!scripts.empty()) {
        runtime.load_scripts(scripts);
    }
    return runtime;
}

script::EventCommand to_command(const network::NamedEvent& event) {
    script::EventCommand command{.name = event.name, .arguments = {}};
    command.arguments.reserve(event.arguments.size());
    for (const auto& argument : event.arguments) {
        command.arguments.emplace_back(argument.key, argument.value);
    }
    return command;
}

constexpr std::size_t assets_per_frame = 4;

template <typename Preload>
void add_all(AssetPreloader& preloader, const network::StringTable& strings,
             network::StringCategory category, Preload preload) {
    for (const auto& entry : strings.entries(category)) {
        preloader.add([preload, name = entry.name] { return preload(name); });
    }
}

} // namespace

struct ClientApp::Impl {
    std::reference_wrapper<graphics::IRenderer> renderer;
    std::reference_wrapper<const graphics::IInput> input;
    std::reference_wrapper<audio::IAudio> audio;
    ClientAppOptions options;
    core::event::EventBus events;
    core::FieldRegistry fields;
    network::NetworkClient network;
    core::EntityManager local;
    script::ScriptRuntime runtime;
    script::ClientApi& client;
    PhysicsStep local_physics;
    AudioPlayer sound;
    TextureLibrary textures;
    ShaderLibrary shaders;
    WorldDraw world_draw;
    HudDraw hud_draw;
    SnapshotView view;
    std::reference_wrapper<const core::time::IClock> time_source;
    core::time::Duration started_at;
    core::time::FixedStepLoop loop;
    AssetPreloader preloader;
    core::metrics::Metrics metrics;
    core::metrics::ProcessMonitor process;
    std::optional<core::time::Duration> previous_frame_at;
    std::optional<std::uint32_t> previous_server_tick;
    bool preload_queued{false};
    bool started{false};

    Impl(const core::time::IClock& clock, network::ClientTransport& transport,
         graphics::IRenderer& window, const graphics::IInput& keys, audio::IAudio& speakers,
         ClientAppOptions settings);

    bool start(std::string& error);
    void frame();
    void stop();

private:
    bool assets_ready();
    void queue_preload();
    void run_ticks(const std::vector<network::EntitySnapshot>& entities);
    void draw(const std::vector<network::EntitySnapshot>& entities, bool playing);
    void draw_world(const std::vector<network::EntitySnapshot>& entities);
    void update_camera(const std::vector<network::EntitySnapshot>& entities);
    void measure_frame_interval();
    void publish_network_gauges();
};

ClientApp::Impl::Impl(const core::time::IClock& clock, network::ClientTransport& transport,
                      graphics::IRenderer& window, const graphics::IInput& keys,
                      audio::IAudio& speakers, ClientAppOptions settings)
    : renderer(window), input(keys), audio(speakers), options(std::move(settings)),
      fields(replicated_fields()), network(clock, events, fields, transport),
      runtime(load_game(local, options.scripts)), client(runtime.api<script::ClientApi>()),
      local_physics(local, events, runtime), sound(speakers, options.assets),
      textures(window, options.assets), shaders(window, options.assets),
      world_draw(window, textures, shaders), hud_draw(window, textures), view(fields),
      time_source(clock), started_at(clock.now()), loop(clock), metrics(clock), process(metrics) {
    client.set_audio_settings(options.audio);
    events.subscribe<network::SoundPlayed>([this](const network::SoundPlayed& played) {
        sound.play_sound(network.strings(), played.sound_id);
    });
    events.subscribe<network::MusicChanged>([this](const network::MusicChanged& changed) {
        sound.change_music(network.strings(), changed.music_id);
    });
    events.subscribe<network::NamedEvent>(
        [this](const network::NamedEvent& event) { client.dispatch(to_command(event)); });
}

bool ClientApp::Impl::start(std::string& error) {
    if (!audio.get().open()) {
        core::log::warn("client: no audio device, sound is disabled");
    }
    std::error_code ec;
    if (!network.connect(ec)) {
        error = "cannot connect: " + ec.message();
        audio.get().close();
        return false;
    }
    if (!renderer.get().open(options.window)) {
        error = "cannot open the window";
        network.disconnect();
        audio.get().close();
        return false;
    }
    started = true;
    return true;
}

void ClientApp::Impl::frame() {
    measure_frame_interval();
    {
        const core::metrics::ScopedTimer timer(metrics, "network.update");
        network.update();
    }
    audio.get().update();
    const bool playing = assets_ready();
    const auto entities = network.entities();
    if (playing) {
        run_ticks(entities);
    }
    sound.apply(client.audio_settings());
    {
        const core::metrics::ScopedTimer timer(metrics, "draw");
        draw(entities, playing);
    }
    publish_network_gauges();
    process.update();
    metrics.end_frame();
}

void ClientApp::Impl::measure_frame_interval() {
    if (!metrics.enabled()) {
        previous_frame_at.reset();
        return;
    }
    const core::time::Duration now = time_source.get().now();
    if (previous_frame_at && now > *previous_frame_at) {
        const core::time::Duration interval = now - *previous_frame_at;
        metrics.record_time("frame", interval);
        metrics.set_gauge("fps", 1.0 / std::chrono::duration<double>(interval).count());
    }
    previous_frame_at = now;
}

void ClientApp::Impl::publish_network_gauges() {
    if (!metrics.enabled()) {
        return;
    }
    const network::NetworkStats stats = network.stats();
    if (stats.round_trip_time) {
        metrics.set_gauge(
            "network.rtt",
            std::chrono::duration<double, std::milli>(*stats.round_trip_time).count());
    }
    metrics.set_gauge("network.packets_lost", static_cast<double>(stats.packets_lost));
    metrics.set_gauge("network.packets_in", static_cast<double>(stats.packets_received_per_second));
    metrics.set_gauge("network.packets_out", static_cast<double>(stats.packets_sent_per_second));
    metrics.set_gauge("network.bytes_in", static_cast<double>(stats.bytes_received_per_second));
    metrics.set_gauge("network.bytes_out", static_cast<double>(stats.bytes_sent_per_second));
    const auto server_tick = network.server_tick();
    if (server_tick && server_tick != previous_server_tick) {
        metrics.count("network.snapshots");
    }
    previous_server_tick = server_tick;
}

bool ClientApp::Impl::assets_ready() {
    if (!preload_queued) {
        if (network.state() != network::ConnectionState::connected) {
            return false;
        }
        queue_preload();
    }
    preloader.load_next(assets_per_frame);
    return preloader.done();
}

void ClientApp::Impl::queue_preload() {
    preload_queued = true;
    const auto strings = network.strings();
    add_all(preloader, strings, network::StringCategory::texture,
            [this](const std::string& name) { return textures.preload(name); });
    add_all(preloader, strings, network::StringCategory::shader,
            [this](const std::string& name) { return shaders.preload(name); });
    add_all(preloader, strings, network::StringCategory::sound,
            [this](const std::string& name) { return sound.preload_sound(name); });
    add_all(preloader, strings, network::StringCategory::music,
            [this](const std::string& name) { return sound.preload_music(name); });
}

void ClientApp::Impl::update_camera(const std::vector<network::EntitySnapshot>& entities) {
    const core::Vec2 viewport = renderer.get().virtual_size();
    client.set_camera_view(resolve_camera(entities, client.camera_override(), viewport), viewport);
}

void ClientApp::Impl::run_ticks(const std::vector<network::EntitySnapshot>& entities) {
    const auto held = graphics::capture_input(input.get());
    update_camera(entities);
    loop.run_frame([&](core::time::Duration step) {
        const float dt = std::chrono::duration<float>(step).count();
        client.set_input(held);
        {
            const core::metrics::ScopedTimer timer(metrics, "lua.update");
            runtime.update(dt);
        }
        {
            const core::metrics::ScopedTimer timer(metrics, "physics");
            local_physics.step(dt);
            runtime.flush_destroyed();
        }
        network.send_input(held);
    });
}

void ClientApp::Impl::draw(const std::vector<network::EntitySnapshot>& entities, bool playing) {
    renderer.get().begin_frame(options.background);
    if (playing) {
        draw_world(entities);
        hud_draw.draw(client.draw_hud());
    } else {
        draw_loading_screen(renderer.get(), preloader.loaded(), preloader.total());
    }
    renderer.get().end_frame();
}

void ClientApp::Impl::draw_world(const std::vector<network::EntitySnapshot>& entities) {
    const core::Vec2 viewport = renderer.get().virtual_size();
    const core::CameraView camera = resolve_camera(entities, client.camera_override(), viewport);
    client.set_camera_view(camera, viewport);
    metrics.set_gauge("entities", static_cast<double>(entities.size()));
    renderer.get().begin_world(camera);
    world_draw.draw({std::ref(view.mirror(entities)), std::ref(local)},
                    std::chrono::duration<float>(time_source.get().now() - started_at).count());
    renderer.get().end_world();
}

void ClientApp::Impl::stop() {
    if (!started) {
        return;
    }
    started = false;
    network.disconnect();
    renderer.get().close();
    audio.get().close();
}

ClientApp::ClientApp(const core::time::IClock& clock, network::ClientTransport& transport,
                     graphics::IRenderer& renderer, const graphics::IInput& input,
                     audio::IAudio& audio, ClientAppOptions options)
    : _impl(std::make_unique<Impl>(clock, transport, renderer, input, audio, std::move(options))) {}

ClientApp::~ClientApp() { _impl->stop(); }

bool ClientApp::start(std::string& error) { return _impl->start(error); }

void ClientApp::frame() { _impl->frame(); }

void ClientApp::run(const std::function<bool()>& should_stop) {
    while (!should_close() && !should_stop()) {
        frame();
    }
    stop();
}

void ClientApp::stop() { _impl->stop(); }

bool ClientApp::should_close() const { return _impl->renderer.get().should_close(); }

network::NetworkClient& ClientApp::network() noexcept { return _impl->network; }

script::ScriptRuntime& ClientApp::script() noexcept { return _impl->runtime; }

script::ClientApi& ClientApp::script_client() noexcept { return _impl->client; }

core::metrics::Metrics& ClientApp::metrics() noexcept { return _impl->metrics; }

} // namespace engine::app
