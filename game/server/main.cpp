#include <csignal>
#include <filesystem>
#include <iostream>

#include "Cli.hpp"
#include "engine/app/Launch.hpp"

namespace {

volatile std::sig_atomic_t stop_requested = 0;

void request_stop(int) { stop_requested = 1; }

} // namespace

int main(int argc, char** argv) {
    const auto result = rtype::server::parse_cli({argv, argv + argc});

    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    engine::app::LaunchOptions options;
    options.mode = engine::app::Mode::dedicated;
    options.game = std::filesystem::path(argv[0]).parent_path();
    options.port = result.config->port;
    options.max_players = result.config->max_players;
    return engine::app::launch(options, [] { return stop_requested != 0; }) ? 0 : 84;
}
