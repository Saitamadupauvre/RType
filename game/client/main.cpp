#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <system_error>

#include "ActionCodec.hpp"
#include "Cli.hpp"
#include "Controls.hpp"
#include "SnapshotView.hpp"
#include "Starfield.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/network/Client.hpp"

namespace {

namespace fs = std::filesystem;

constexpr engine::graphics::Color background{.r = 0, .g = 0, .b = 0, .a = 255};
constexpr std::uint32_t starfield_seed = 1987;

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

    const engine::core::time::SteadyClock clock;
    engine::core::event::EventBus events;
    engine::network::NetworkClient network(clock, events);
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
    rtype::client::SnapshotView view(renderer, executable_dir / "assets");

    while (!renderer.should_close()) {
        network.update();
        renderer.begin_frame(background);
        const auto controls = rtype::client::read_controls(input);
        loop.run_frame([&](engine::core::time::Duration step) {
            starfield.update(std::chrono::duration<float>(step).count());
            const auto sent = rtype::common::pack_input(controls);
            network.send_input(sent.action_flags, sent.move_x, sent.move_y);
        });
        starfield.draw(renderer);
        view.draw(network.entities(), network.types(), network.player_id());
        renderer.end_frame();
    }

    network.disconnect();
    return 0;
}
