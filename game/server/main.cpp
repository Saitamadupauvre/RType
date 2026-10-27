#include "Cli.hpp"
#include "ServerApp.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>

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

    const engine::core::time::SteadyClock clock;
    const std::filesystem::path scripts = std::filesystem::path(argv[0]).parent_path() / "scripts";
    rtype::server::ServerApp app(clock, *result.config, scripts);
    std::string error;
    if (!app.start(error)) {
        std::cerr << error << "\n";
        return 84;
    }

    std::signal(SIGINT, request_stop);
    std::signal(SIGTERM, request_stop);
    std::cout << "Listening on UDP port " << app.transport().port() << "\n";
    app.run([] { return stop_requested != 0; });
    return 0;
}
