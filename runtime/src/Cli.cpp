#include "Cli.hpp"
#include "clap.hpp"
#include <cstdint>

namespace runtime {

namespace {

using engine::app::Mode;

constexpr int error_exit_code = 84;

CliResult rejected(const std::string& message) {
    return {.config = std::nullopt, .exit_code = error_exit_code, .message = message + "\n"};
}

} // namespace

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "engine-runtime" : args[0];
    clap::App app(program, "Runs a game made with the engine");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& server_ip = app.positional<std::string>("server-ip", "Address of the server to join.")
                          .validator(clap::NonEmpty)
                          .default_value("");
    auto& port = app.positional<std::uint16_t>("port", "UDP port of the server to join.")
                     .range(1, 65535)
                     .default_value(0);
    auto& hosted = app.option<std::uint16_t>(
                          "--host", "Host a game on this UDP port and play on it; others can join.")
                       .range(1, 65535);
    auto& solo = app.flag("--solo", "Play alone, without any network.");
    auto& dedicated =
        app.option<std::uint16_t>("--server", "Run a dedicated server on this UDP port, no window.")
            .range(1, 65535);
    auto& max_players =
        app.option<std::size_t>("-p,--max-players", "Maximum connected players when hosting.")
            .range(1, max_supported_players)
            .default_value(max_supported_players);
    auto& music_volume = app.option<unsigned int>("--music-volume", "Music volume, from 0 to 100.")
                             .range(0, 100)
                             .default_value(100);
    auto& effects_volume =
        app.option<unsigned int>("--effects-volume", "Sound effects volume, from 0 to 100.")
            .range(0, 100)
            .default_value(100);
    auto& muted = app.flag("--mute", "Start with every sound muted.");
    auto& game = app.option<std::string>("--game", "Game folder to run instead of the one next "
                                                   "to this program.")
                     .validator(clap::NonEmpty)
                     .default_value("");
    auto& scene_file =
        app.option<std::string>("--scene-file", "Scene file to start from instead of the game's "
                                                "start scene, such as one the editor plays.")
            .validator(clap::NonEmpty)
            .default_value("");

    app.example(program + " --solo", "Play alone.");
    app.example(program + " --host 4242", "Host a game on port 4242 and play on it.");
    app.example(program + " 127.0.0.1 4242", "Join a server on port 4242.");
    app.example(program + " --server 4242 --max-players 2", "Run a server for two players.");
    app.example(program + " --solo --game examples/r-type", "Play another game folder.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : error_exit_code,
                .message = help ? app.help() : app.error()};
    }

    const bool remote = server_ip.is_set() || port.is_set();
    const int chosen = (remote ? 1 : 0) + (hosted.is_set() ? 1 : 0) + (solo ? 1 : 0) +
                       (dedicated.is_set() ? 1 : 0);
    if (chosen != 1) {
        return rejected(
            "Give either <server-ip> <port>, --host <port>, --server <port> or --solo.");
    }
    if (remote && !(server_ip.is_set() && port.is_set())) {
        return rejected("Missing required argument: port");
    }

    RuntimeConfig config{.mode = Mode::client,
                         .server_ip = server_ip.get(),
                         .port = port.get(),
                         .max_players = max_players.get(),
                         .music_volume = music_volume.get(),
                         .effects_volume = effects_volume.get(),
                         .muted = muted,
                         .game = game.get(),
                         .scene_file = scene_file.get()};
    if (hosted.is_set()) {
        config.mode = Mode::host;
        config.port = hosted.get();
    } else if (dedicated.is_set()) {
        config.mode = Mode::dedicated;
        config.port = dedicated.get();
    } else if (solo) {
        config.mode = Mode::solo;
    }
    return {.config = config, .exit_code = 0, .message = ""};
}

} // namespace runtime
