#include "engine/app/ClientApp.hpp"

#include <chrono>
#include <system_error>
#include <utility>

#include "AudioPlayer.hpp"
#include "Fields.hpp"
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
        command.arguments.push_back({.key = argument.key, .value = argument.value});
    }
    return command;
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
    bool started{false};

    Impl(const core::time::IClock& clock, network::ClientTransport& transport,
         graphics::IRenderer& window, const graphics::IInput& keys, audio::IAudio& speakers,
         ClientAppOptions settings);

    bool start(std::string& error);
    void frame();
    void stop();
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
    const auto local = graphics::capture_input(input.get());
    loop.run_frame([&](core::time::Duration step) {
        client.set_input(local);
        runtime.update(std::chrono::duration<float>(step).count());
        network.send_input(local);
    });
    sound.apply(client.audio_settings());
    renderer.get().begin_frame(options.background);
    view.draw(network.entities(), network.strings());
    renderer.get().end_frame();
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
