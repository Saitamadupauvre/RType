#include "Cli.hpp"
#include "clap.hpp"
#include <cstdint>

namespace rtype::client {

namespace {

using engine::app::Mode;

constexpr int error_exit_code = 84;

CliResult rejected(const std::string& message) {
    return {.config = std::nullopt, .exit_code = error_exit_code, .message = message + "\n"};
}

} // namespace

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "r-type_client" : args[0];
    clap::App app(program, "The client for the R-Type project");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& host = app.positional<std::string>("server-ip", "Address of the server.")
                     .validator(clap::NonEmpty)
                     .default_value("");
    auto& port = app.positional<std::uint16_t>("port", "UDP port of the server.")
                     .range(1, 65535)
                     .default_value(0);
    auto& hosted = app.option<std::uint16_t>(
                          "--host", "Host a game on this UDP port and play on it; others can join.")
                       .range(1, 65535);
    auto& solo = app.flag("--solo", "Play alone, without any network.");
    auto& music_volume = app.option<unsigned int>("--music-volume", "Music volume, from 0 to 100.")
                             .range(0, 100)
                             .default_value(100);
    auto& effects_volume =
        app.option<unsigned int>("--effects-volume", "Sound effects volume, from 0 to 100.")
            .range(0, 100)
            .default_value(100);
    auto& muted = app.flag("--mute", "Start with every sound muted (toggle in game with M).");

    app.example(program + " 127.0.0.1 4242", "Connect to a local server on port 4242.");
    app.example(program + " 127.0.0.1 4242 --music-volume 30",
                "Connect with the music at 30% volume.");
    app.example(program + " --host 4242", "Host a game on port 4242 and play on it.");
    app.example(program + " --solo", "Play alone.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : error_exit_code,
                .message = help ? app.help() : app.error()};
    }

    const bool remote = host.is_set() || port.is_set();
    const int chosen = (remote ? 1 : 0) + (hosted.is_set() ? 1 : 0) + (solo ? 1 : 0);
    if (chosen != 1) {
        return rejected("Give either <server-ip> <port>, --host <port> or --solo.");
    }
    if (remote && !(host.is_set() && port.is_set())) {
        return rejected("Missing required argument: port");
    }

    ClientConfig config{.mode = Mode::client,
                        .host = host.get(),
                        .port = port.get(),
                        .music_volume = music_volume.get(),
                        .effects_volume = effects_volume.get(),
                        .muted = muted};
    if (hosted.is_set()) {
        config.mode = Mode::host;
        config.port = hosted.get();
    } else if (solo) {
        config.mode = Mode::solo;
    }
    return {.config = config, .exit_code = 0, .message = ""};
}

} // namespace rtype::client
