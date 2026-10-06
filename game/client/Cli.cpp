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

    app.example(program + " 127.0.0.1 4242", "Connect to a local server on port 4242.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : 84,
                .message = help ? app.help() : app.error()};
    }

    return {.config = ClientConfig{.host = host.get(), .port = port.get()},
            .exit_code = 0,
            .message = ""};
}

} // namespace rtype::client
