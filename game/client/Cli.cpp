#include "Cli.hpp"
#include "clap.hpp"
#include <cstdint>

namespace rtype::client {

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "r-type_client" : args[0];
    clap::App app(program, "The client for the R-Type project");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& host = app.positional<std::string>("server-ip", "Address of the server.")
                     .validator(clap::NonEmpty);
    auto& port = app.positional<std::uint16_t>("port", "UDP port of the server.").range(1, 65535);
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

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : 84,
                .message = help ? app.help() : app.error()};
    }

    return {.config = ClientConfig{.host = host.get(),
                                   .port = port.get(),
                                   .music_volume = music_volume.get(),
                                   .effects_volume = effects_volume.get(),
                                   .muted = muted},
            .exit_code = 0,
            .message = ""};
}

} // namespace rtype::client
