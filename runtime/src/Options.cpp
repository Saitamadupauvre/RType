#include "Options.hpp"

#include "engine/script/GameFolder.hpp"

namespace runtime {

namespace {

namespace fs = std::filesystem;

constexpr float percent = 100.F;
constexpr int window_width = 1280;
constexpr int window_height = 720;

float level(unsigned int volume) { return static_cast<float>(volume) / percent; }

} // namespace

std::string window_title(const fs::path& game) {
    const auto project = engine::script::read_project(game);
    if (project && project->name && !project->name->empty()) {
        return *project->name;
    }
    const fs::path folder = fs::absolute(game).lexically_normal();
    const fs::path name =
        folder.has_filename() ? folder.filename() : folder.parent_path().filename();
    return name.empty() ? std::string("Game") : name.string();
}

engine::app::LaunchOptions launch_options(const RuntimeConfig& config,
                                          const fs::path& executable_dir,
                                          const PluginFiles& plugins) {
    const fs::path game = config.game.empty() ? executable_dir : fs::path(config.game);
    return {.mode = config.mode,
            .game = game,
            .start_scene_file = config.scene_file,
            .graphics_plugin = executable_dir / "plugins" / plugins.graphics,
            .audio_plugin = executable_dir / "plugins" / plugins.audio,
            .server_ip = config.server_ip,
            .port = config.port,
            .max_players = config.max_players,
            .window = {.width = window_width,
                       .height = window_height,
                       .title = window_title(game),
                       .fullscreen = false},
            .audio = {.music_volume = level(config.music_volume),
                      .effects_volume = level(config.effects_volume),
                      .muted = config.muted}};
}

} // namespace runtime
