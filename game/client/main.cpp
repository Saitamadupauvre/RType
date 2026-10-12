#include "Cli.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/graphics/Types.hpp"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    const auto result = rtype::client::parse_cli({argv, argv + argc});

    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }

    auto plugin_path =
        std::filesystem::path(argv[0]).parent_path() / "plugins" / GRAPHICS_PLUGIN_FILE;

    auto lib_result = engine::graphics::load_graphics_plugin(plugin_path);
    if (!lib_result.plugin) {
        std::cerr << lib_result.error << "\n";
        return 84;
    }
    auto& renderer = lib_result.plugin->backend().renderer();
    if (!renderer.open({})) {
        std::cerr << "Failed to open window\n";
        return 84;
    }

    auto blue = engine::graphics::Color{.r = 0, .g = 0, .b = 255, .a = 255};

    while (!renderer.should_close()) {
        renderer.begin_frame(blue);
        renderer.end_frame();
    }

    return 0;
}
