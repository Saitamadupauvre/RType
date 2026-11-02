#include "engine/app/ClientApp.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <system_error>
#include <utility>

#include "AssetPreloader.hpp"
#include "AudioPlayer.hpp"
#include "CameraRig.hpp"
#include "Fields.hpp"
#include "LoadingScreen.hpp"
#include "SnapshotView.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/graphics/InputCapture.hpp"
#include "engine/network/Events.hpp"
#include "engine/script/EventCommand.hpp"

namespace engine::app {

namespace {

script::ScriptRuntime load_game(const std::filesystem::path& scripts) {
    auto runtime = script::ScriptRuntime::builder().with<script::ClientApi>().build();
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
    script::ScriptRuntime runtime;
    script::ClientApi& client;
    AudioPlayer sound;
    SnapshotView view;
    core::time::FixedStepLoop loop;
    AssetPreloader preloader;
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
    void run_ticks();
    void draw(bool playing);
    void draw_world();
};

ClientApp::Impl::Impl(const core::time::IClock& clock, network::ClientTransport& transport,
                      graphics::IRenderer& window, const graphics::IInput& keys,
                      audio::IAudio& speakers, ClientAppOptions settings)
    : renderer(window), input(keys), audio(speakers), options(std::move(settings)),
      fields(replicated_fields()), network(clock, events, fields, transport),
      runtime(load_game(options.scripts)), client(runtime.api<script::ClientApi>()),
      sound(speakers, options.assets), view(window, options.assets, fields), loop(clock) {
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
    network.update();
    audio.get().update();
    const bool playing = assets_ready();
    if (playing) {
        run_ticks();
    }
    sound.apply(client.audio_settings());
    draw(playing);
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
            [this](const std::string& name) { return view.preload_texture(name); });
    add_all(preloader, strings, network::StringCategory::sound,
            [this](const std::string& name) { return sound.preload_sound(name); });
    add_all(preloader, strings, network::StringCategory::music,
            [this](const std::string& name) { return sound.preload_music(name); });
}

void ClientApp::Impl::run_ticks() {
    const auto local = graphics::capture_input(input.get());
    const core::Vec2 viewport = renderer.get().virtual_size();
    const auto entities = network.entities();
    client.set_camera_view(resolve_camera(entities, client.camera_override(), viewport), viewport);
    loop.run_frame([&](core::time::Duration step) {
        client.set_input(local);
        runtime.update(std::chrono::duration<float>(step).count());
        network.send_input(local);
    });
}

void ClientApp::Impl::draw(bool playing) {
    renderer.get().begin_frame(options.background);
    if (playing) {
        draw_world();
    } else {
        draw_loading_screen(renderer.get(), preloader.loaded(), preloader.total());
    }
    renderer.get().end_frame();
}

void ClientApp::Impl::draw_world() {
    const core::Vec2 viewport = renderer.get().virtual_size();
    const auto entities = network.entities();
    const core::CameraView camera = resolve_camera(entities, client.camera_override(), viewport);
    client.set_camera_view(camera, viewport);
    renderer.get().begin_world(camera);
    view.draw(entities);
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

} // namespace engine::app
