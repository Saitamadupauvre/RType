#include "engine/app/ClientApp.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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
#include "engine/app/ServerApp.hpp"
#include "engine/core/console/LogCapture.hpp"
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
#include "render/ConsoleOverlay.hpp"
#include "render/HitboxDraw.hpp"
#include "render/HudDraw.hpp"
#include "render/LoadingScreen.hpp"
#include "render/MetricsOverlay.hpp"
#include "render/SnapshotView.hpp"
#include "render/WorldDraw.hpp"
#include "simulation/PhysicsStep.hpp"

namespace engine::app {

namespace {

namespace console = core::console;

constexpr std::string_view metrics_cvar = "debug.metrics";
constexpr std::string_view hitboxes_cvar = "debug.hitboxes";
constexpr std::string_view music_cvar = "audio.music_volume";
constexpr std::string_view effects_cvar = "audio.effects_volume";
constexpr std::string_view muted_cvar = "audio.muted";
constexpr int console_page = 5;

class ConsoleLog {
public:
    ConsoleLog() : _capture(std::make_shared<console::LogCapture>(core::log::current_sink())) {
        core::log::set_sink(_capture);
    }

    ConsoleLog(const ConsoleLog&) = delete;
    ConsoleLog& operator=(const ConsoleLog&) = delete;
    ConsoleLog(ConsoleLog&&) = delete;
    ConsoleLog& operator=(ConsoleLog&&) = delete;

    ~ConsoleLog() {
        if (core::log::current_sink() == _capture) {
            core::log::set_sink(_capture->next());
        }
    }

    void print_to(console::Console& target) {
        for (const auto& record : _capture->take()) {
            const bool failure =
                record.level == core::log::Level::Warn || record.level == core::log::Level::Error;
            target.print(failure ? console::LineKind::error : console::LineKind::log,
                         record.message);
        }
    }

private:
    std::shared_ptr<console::LogCapture> _capture;
};

script::ScriptRuntime load_game(core::EntityManager& local, console::CVars& cvars,
                                const std::filesystem::path& scripts) {
    auto runtime = script::ScriptRuntime::builder().with<script::ClientApi>().build();
    runtime.attach_world(local);
    runtime.attach_cvars(cvars);
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
    std::reference_wrapper<graphics::IInput> input;
    std::reference_wrapper<audio::IAudio> audio;
    ClientAppOptions options;
    core::event::EventBus events;
    core::FieldRegistry fields;
    network::NetworkClient network;
    core::EntityManager local;
    ConsoleLog console_log;
    console::CVars cvars;
    console::Console console{cvars};
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
    const core::metrics::Metrics* server_metrics{nullptr};
    ServerApp* local_server{nullptr};
    bool preload_queued{false};
    bool started{false};

    Impl(const core::time::IClock& clock, network::ClientTransport& transport,
         graphics::IRenderer& window, graphics::IInput& keys, audio::IAudio& speakers,
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
    void toggle_overlay();
    void publish_network_gauges();
    void register_cvars();
    void add_cvar(console::CVarSpec spec, console::CVars::Listener on_change = {});
    void apply_audio_cvars();
    void follow_audio_settings();
    void update_console();
    void toggle_console();
    void edit_console();
    void type_into_console();
    void drop_typed_characters();
    void print_server_results();
    [[nodiscard]] core::InputState game_input() const;
};

ClientApp::Impl::Impl(const core::time::IClock& clock, network::ClientTransport& transport,
                      graphics::IRenderer& window, graphics::IInput& keys, audio::IAudio& speakers,
                      ClientAppOptions settings)
    : renderer(window), input(keys), audio(speakers), options(std::move(settings)),
      fields(replicated_fields()), network(clock, events, fields, transport),
      runtime(load_game(local, cvars, options.scripts)), client(runtime.api<script::ClientApi>()),
      local_physics(local, events, runtime), sound(speakers, options.assets),
      textures(window, options.assets), shaders(window, options.assets),
      world_draw(window, textures, shaders), hud_draw(window, textures), view(fields),
      time_source(clock), started_at(clock.now()), loop(clock), metrics(clock), process(metrics) {
    client.set_audio_settings(options.audio);
    register_cvars();
    console.set_lua([this](std::string_view code) { return runtime.evaluate(code); });
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

void ClientApp::Impl::register_cvars() {
    const script::AudioSettings audio_levels = client.audio_settings();
    const console::CVarRange volume{.min = 0.0, .max = 1.0};
    const auto on_audio = [this](const console::CVarValue&) { apply_audio_cvars(); };
    add_cvar({.name = std::string(metrics_cvar),
              .default_value = false,
              .description = "show the metrics overlay (F3)",
              .range = std::nullopt});
    add_cvar({.name = std::string(hitboxes_cvar),
              .default_value = false,
              .description = "outline the colliders of the client's local entities",
              .range = std::nullopt});
    add_cvar({.name = std::string(music_cvar),
              .default_value = static_cast<double>(audio_levels.music_volume),
              .description = "music volume",
              .range = volume},
             on_audio);
    add_cvar({.name = std::string(effects_cvar),
              .default_value = static_cast<double>(audio_levels.effects_volume),
              .description = "sound effects volume",
              .range = volume},
             on_audio);
    add_cvar({.name = std::string(muted_cvar),
              .default_value = audio_levels.muted,
              .description = "mute every sound",
              .range = std::nullopt},
             on_audio);
}

void ClientApp::Impl::add_cvar(console::CVarSpec spec, console::CVars::Listener on_change) {
    std::string name = spec.name;
    if (const auto added = cvars.add(std::move(spec), std::move(on_change)); !added) {
        core::log::error("client: cannot register {}: {}", name, added.error);
    }
}

void ClientApp::Impl::apply_audio_cvars() {
    client.set_audio_settings(
        {.music_volume = static_cast<float>(cvars.get_number(music_cvar, 1.0)),
         .effects_volume = static_cast<float>(cvars.get_number(effects_cvar, 1.0)),
         .muted = cvars.get_bool(muted_cvar, false)});
}

void ClientApp::Impl::follow_audio_settings() {
    const script::AudioSettings levels = client.audio_settings();
    static_cast<void>(cvars.set(music_cvar, static_cast<double>(levels.music_volume)));
    static_cast<void>(cvars.set(effects_cvar, static_cast<double>(levels.effects_volume)));
    static_cast<void>(cvars.set(muted_cvar, levels.muted));
}

void ClientApp::Impl::update_console() {
    console_log.print_to(console);
    print_server_results();
    toggle_console();
    if (console.is_open()) {
        edit_console();
        type_into_console();
    } else {
        drop_typed_characters();
    }
}

void ClientApp::Impl::toggle_console() {
    const graphics::IInput& keys = input.get();
    const bool toggle =
        keys.is_key_pressed(core::Key::GraveAccent) || keys.is_key_pressed(core::Key::F1);
    if (toggle || (console.is_open() && keys.is_key_pressed(core::Key::Escape))) {
        console.set_open(toggle && !console.is_open());
        drop_typed_characters();
    }
}

void ClientApp::Impl::edit_console() {
    const graphics::IInput& keys = input.get();
    const auto pressed = [&keys](core::Key key) { return keys.is_key_pressed(key); };
    if (pressed(core::Key::Backspace)) {
        console.erase_before();
    }
    if (pressed(core::Key::Delete)) {
        console.erase_after();
    }
    if (pressed(core::Key::Left)) {
        console.move_left();
    }
    if (pressed(core::Key::Right)) {
        console.move_right();
    }
    if (pressed(core::Key::Home)) {
        console.move_home();
    }
    if (pressed(core::Key::End)) {
        console.move_end();
    }
    if (pressed(core::Key::Up)) {
        console.history_previous();
    }
    if (pressed(core::Key::Down)) {
        console.history_next();
    }
    if (pressed(core::Key::PageUp)) {
        console.scroll(console_page);
    }
    if (pressed(core::Key::PageDown)) {
        console.scroll(-console_page);
    }
}

void ClientApp::Impl::type_into_console() {
    for (char32_t typed = input.get().next_character(); typed != U'\0';
         typed = input.get().next_character()) {
        console.type(typed);
    }
    const graphics::IInput& keys = input.get();
    if (keys.is_key_pressed(core::Key::Enter) || keys.is_key_pressed(core::Key::NumpadEnter)) {
        console.submit();
    }
}

void ClientApp::Impl::drop_typed_characters() {
    char32_t typed = input.get().next_character();
    while (typed != U'\0') {
        typed = input.get().next_character();
    }
}

void ClientApp::Impl::print_server_results() {
    if (local_server == nullptr) {
        return;
    }
    for (const auto& result : local_server->take_lua_results()) {
        if (!result.output.empty()) {
            console.print(result.ok ? console::LineKind::output : console::LineKind::error,
                          result.output);
        }
    }
}

core::InputState ClientApp::Impl::game_input() const {
    if (console.is_open()) {
        return {};
    }
    return graphics::capture_input(input.get());
}

void ClientApp::Impl::frame() {
    measure_frame_interval();
    update_console();
    toggle_overlay();
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
    follow_audio_settings();
    sound.apply(client.audio_settings());
    {
        const core::metrics::ScopedTimer timer(metrics, "draw");
        draw(entities, playing);
    }
    publish_network_gauges();
    process.update();
    metrics.end_frame();
}

void ClientApp::Impl::toggle_overlay() {
    if (!console.is_open() && input.get().is_key_pressed(core::Key::F3)) {
        static_cast<void>(cvars.set(metrics_cvar, !cvars.get_bool(metrics_cvar, false)));
    }
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
    const auto held = game_input();
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
    if (cvars.get_bool(metrics_cvar, false)) {
        draw_metrics_overlay(renderer.get(), {.client = metrics, .server = server_metrics});
    }
    if (console.is_open()) {
        draw_console(renderer.get(), console);
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
    if (cvars.get_bool(hitboxes_cvar, false)) {
        draw_hitboxes(renderer.get(), local);
    }
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
                     graphics::IRenderer& renderer, graphics::IInput& input, audio::IAudio& audio,
                     ClientAppOptions options)
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

void ClientApp::show_metrics_overlay(bool shown) {
    static_cast<void>(_impl->cvars.set(metrics_cvar, shown));
}

bool ClientApp::metrics_overlay_shown() const noexcept {
    return _impl->cvars.get_bool(metrics_cvar, false);
}

void ClientApp::show_server_metrics(const core::metrics::Metrics* server) noexcept {
    _impl->server_metrics = server;
}

void ClientApp::attach_local_server(ServerApp& server) {
    _impl->server_metrics = &server.metrics();
    _impl->local_server = &server;
    _impl->console.set_server_lua([&server](std::string_view code) {
        server.submit_lua(std::string(code));
        return core::console::ExecutionResult{};
    });
}

core::console::Console& ClientApp::console() noexcept { return _impl->console; }

core::console::CVars& ClientApp::cvars() noexcept { return _impl->cvars; }

} // namespace engine::app
