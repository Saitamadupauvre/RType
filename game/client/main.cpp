#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>

#include "AudioPlayer.hpp"
#include "Cli.hpp"
#include "GameFields.hpp"
#include "SnapshotView.hpp"
#include "Starfield.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/audio/NullAudio.hpp"
#include "engine/audio/PluginLoader.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/graphics/InputCapture.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Events.hpp"

namespace {

namespace fs = std::filesystem;

constexpr engine::graphics::Color background{.r = 0, .g = 0, .b = 0, .a = 255};
constexpr std::uint32_t starfield_seed = 1987;
constexpr float percent = 100.F;

rtype::client::AudioSettings audio_settings(const rtype::client::ClientConfig& config) {
    return {.music_volume = static_cast<float>(config.music_volume) / percent,
            .effects_volume = static_cast<float>(config.effects_volume) / percent,
            .muted = config.muted};
}

} // namespace

int main(int argc, char** argv) {
    const auto result = rtype::client::parse_cli({argv, argv + argc});

    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }

    const fs::path executable_dir = fs::path(argv[0]).parent_path();

    const fs::path plugin_path = executable_dir / "plugins" / GRAPHICS_PLUGIN_FILE;
    auto lib_result = engine::graphics::load_graphics_plugin(plugin_path);
    if (!lib_result.plugin) {
        std::cerr << lib_result.error << "\n";
        return 84;
    }
    auto& renderer = lib_result.plugin->backend().renderer();
    const auto& input = lib_result.plugin->backend().input();

    auto audio_plugin =
        engine::audio::load_audio_plugin(executable_dir / "plugins" / AUDIO_PLUGIN_FILE);
    engine::audio::NullAudio silence;
    if (!audio_plugin.plugin) {
        std::cerr << audio_plugin.error << ": sound is disabled\n";
    }
    engine::audio::IAudio& audio = audio_plugin.plugin ? audio_plugin.plugin->backend() : silence;
    audio.open();
    rtype::client::AudioPlayer sound(audio, executable_dir / "assets",
                                     audio_settings(*result.config));

    const engine::core::time::SteadyClock clock;
    engine::core::event::EventBus events;
    const auto fields = rtype::common::game_fields();
    engine::network::NetworkClient network(clock, events, fields);
    events.subscribe<engine::network::SoundPlayed>([&](const engine::network::SoundPlayed& played) {
        sound.play_sound(network.strings(), played.sound_id);
    });
    events.subscribe<engine::network::MusicChanged>(
        [&](const engine::network::MusicChanged& changed) {
            sound.change_music(network.strings(), changed.music_id);
        });
    std::error_code ec;
    if (!network.connect(result.config->host, result.config->port, ec)) {
        std::cerr << "cannot connect to " << result.config->host << ":" << result.config->port
                  << ": " << ec.message() << "\n";
        return 84;
    }

    if (!renderer.open({.width = 1280, .height = 720, .title = "R-Type", .fullscreen = false})) {
        std::cerr << "Failed to open window\n";
        network.disconnect();
        return 84;
    }

    engine::core::time::FixedStepLoop loop(clock);
    rtype::client::Starfield starfield(starfield_seed, renderer.virtual_size());
    rtype::client::SnapshotView view(renderer, executable_dir / "assets", fields);

    while (!renderer.should_close()) {
        network.update();
        audio.update();
        renderer.begin_frame(background);
        if (input.is_key_pressed(engine::graphics::Key::M)) {
            sound.toggle_mute();
        }
        const auto controls = engine::graphics::capture_input(input);
        loop.run_frame([&](engine::core::time::Duration step) {
            starfield.update(std::chrono::duration<float>(step).count());
            network.send_input(controls);
        });
        starfield.draw(renderer);
        view.draw(network.entities(), network.strings(), network.player_id());
        renderer.end_frame();
    }

    network.disconnect();
    return 0;
}
