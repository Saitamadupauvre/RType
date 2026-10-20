#include <chrono>
#include <filesystem>
#include <iostream>

#include "Cli.hpp"
#include "Controls.hpp"
#include "WorldDraw.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace {

namespace fs = std::filesystem;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Tags;

constexpr engine::graphics::Color background{.r = 0, .g = 0, .b = 0, .a = 255};

void give_controls_to_players(EntityManager& world, engine::script::ScriptRuntime& runtime,
                              const engine::script::InputState& controls) {
    world.query<Tags>().each([&](Entity entity, const Tags& tags) {
        if (tags.has("player")) {
            runtime.set_input(entity, controls);
        }
    });
}

void tick(EntityManager& world, engine::script::ScriptRuntime& runtime,
          const engine::script::InputState& controls, float dt) {
    give_controls_to_players(world, runtime, controls);
    runtime.update(dt);
    runtime.flush_destroyed();
}

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
    if (!renderer.open({.width = 1280, .height = 720, .title = "R-Type", .fullscreen = false})) {
        std::cerr << "Failed to open window\n";
        return 84;
    }

    EntityManager world;
    engine::script::ScriptRuntime runtime;
    runtime.attach_world(world);
    runtime.load_scripts(executable_dir / "scripts");

    const engine::core::time::SteadyClock clock;
    engine::core::time::FixedStepLoop loop(clock);
    rtype::client::WorldDraw world_draw(renderer, executable_dir / "assets");

    while (!renderer.should_close()) {
        renderer.begin_frame(background);
        const engine::script::InputState controls = rtype::client::read_controls(input);
        loop.run_frame([&](engine::core::time::Duration step) {
            tick(world, runtime, controls, std::chrono::duration<float>(step).count());
        });
        world_draw.draw(world);
        renderer.end_frame();
    }

    return 0;
}
