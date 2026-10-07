#include "Cli.hpp"
#include "clap.hpp"
#include <cstdint>

namespace rtype::server {

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "r-type_server" : args[0];
    clap::App app(program, "The server for the R-Type project");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& port = app.positional<std::uint16_t>("port", "UDP port to listen on.").range(1, 65535);

    app.example(program + " 4242", "Start the server on port 4242.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : 84,
                .message = help ? app.help() : app.error()};
    }

    return {.config = ServerConfig{.port = port.get()}, .exit_code = 0, .message = ""};
}

} // namespace rtype::server
