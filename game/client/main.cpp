#include <csignal>
#include <filesystem>
#include <iostream>

#include "Cli.hpp"
#include "engine/app/Launch.hpp"

namespace {

namespace fs = std::filesystem;

constexpr float percent = 100.F;

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) { stop_requested = 1; }

engine::app::LaunchOptions launch_options(const rtype::client::ClientConfig& config,
                                          const fs::path& executable_dir) {
    return {.mode = config.mode,
            .game = executable_dir,
            .graphics_plugin = executable_dir / "plugins" / GRAPHICS_PLUGIN_FILE,
            .audio_plugin = executable_dir / "plugins" / AUDIO_PLUGIN_FILE,
            .server_ip = config.host,
            .port = config.port,
            .max_players = rtype::client::max_players,
            .window = {.width = 1280, .height = 720, .title = "R-Type", .fullscreen = false},
            .audio = {.music_volume = static_cast<float>(config.music_volume) / percent,
                      .effects_volume = static_cast<float>(config.effects_volume) / percent,
                      .muted = config.muted}};
}

} // namespace

int main(int argc, char** argv) {
    const auto result = rtype::client::parse_cli({argv, argv + argc});

    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    const auto options = launch_options(*result.config, fs::path(argv[0]).parent_path());
    return engine::app::launch(options, [] { return stop_requested != 0; }) ? 0 : 84;
}
