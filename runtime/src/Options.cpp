#include "Options.hpp"

#include <algorithm>
#include <format>
#include <string_view>
#include <vector>

#include "engine/script/GameFolder.hpp"

namespace runtime {

namespace {

namespace fs = std::filesystem;

constexpr float percent = 100.F;
constexpr int window_width = 1280;
constexpr int window_height = 720;

float level(unsigned int volume) { return static_cast<float>(volume) / percent; }

std::string_view mode_name(engine::app::Mode mode) {
    switch (mode) {
    case engine::app::Mode::dedicated:
        return "dedicated";
    case engine::app::Mode::client:
        return "client";
    case engine::app::Mode::host:
        return "host";
    case engine::app::Mode::solo:
        return "solo";
    }
    return "solo";
}

std::string joined(const std::vector<std::string>& names) {
    std::string text;
    for (const std::string& name : names) {
        text += text.empty() ? name : ", " + name;
    }
    return text;
}

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

fs::path game_folder(const RuntimeConfig& config, const fs::path& executable_dir) {
    return config.game.empty() ? executable_dir : fs::path(config.game);
}

std::optional<std::string> refused_mode(const RuntimeConfig& config, const fs::path& game) {
    const auto project = engine::script::read_project(game);
    if (!project || !project->modes) {
        return std::nullopt;
    }
    const std::string_view mode = mode_name(config.mode);
    if (std::ranges::find(*project->modes, mode) != project->modes->end()) {
        return std::nullopt;
    }
    return std::format("{} does not offer the {} mode; it offers: {}", window_title(game), mode,
                       joined(*project->modes));
}

engine::app::LaunchOptions launch_options(const RuntimeConfig& config,
                                          const fs::path& executable_dir,
                                          const PluginFiles& plugins) {
    const fs::path game = game_folder(config, executable_dir);
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
