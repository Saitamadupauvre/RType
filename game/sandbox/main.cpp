#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iostream>

#include "Starfield.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/graphics/Types.hpp"

namespace {

namespace fs = std::filesystem;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Key;

constexpr Color background{.r = 0, .g = 0, .b = 16, .a = 255};
constexpr Color white{.r = 255, .g = 255, .b = 255, .a = 255};
constexpr Color red{.r = 220, .g = 40, .b = 40, .a = 255};
constexpr std::uint32_t starfield_seed = 1987;
constexpr float text_size = 32.F;

} // namespace

int main(int, char** argv) {
    const fs::path executable_dir = fs::path(argv[0]).parent_path();
    auto loaded =
        engine::graphics::load_graphics_plugin(executable_dir / "plugins" / GRAPHICS_PLUGIN_FILE);
    if (!loaded.plugin) {
        std::cerr << loaded.error << "\n";
        return 84;
    }
    auto& renderer = loaded.plugin->backend().renderer();
    auto& input = loaded.plugin->backend().input();
    if (!renderer.open({.width = 1280, .height = 720, .title = "R-Type sandbox"})) {
        std::cerr << "Failed to open window\n";
        return 84;
    }

    rtype::client::Starfield starfield(starfield_seed, renderer.virtual_size());
    const engine::graphics::TextureId missing =
        renderer.load_texture((executable_dir / "assets" / "missing.png").string());
    const engine::core::time::SteadyClock clock;
    engine::core::time::FixedStepLoop loop(clock);

    while (!renderer.should_close() && !input.is_key_pressed(Key::Escape)) {
        loop.run_frame([&](engine::core::time::Duration step) {
            starfield.update(std::chrono::duration<float>(step).count());
        });
        const Vec2 mouse = input.mouse_position();

        renderer.begin_frame(background);
        starfield.draw(renderer);
        renderer.draw_sprite({.texture = missing,
                              .source = {.width = 32, .height = 32},
                              .dest = {.x = 100, .y = 100, .width = 128, .height = 128}});
        renderer.draw_rect({.x = 300, .y = 100, .width = 200, .height = 128}, red, false);
        renderer.draw_line({300, 100}, {500, 228}, 4.F, red);
        renderer.draw_circle(mouse, 12.F, red);
        renderer.draw_text(std::format("mouse {:.0f} {:.0f}", mouse.x, mouse.y), {100, 300},
                           text_size, white);
        renderer.end_frame();
    }

    return 0;
}
