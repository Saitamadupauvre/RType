#include "project/ProjectFiles.hpp"

#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

#include "engine/script/GameFolder.hpp"
#include "engine/script/Scene.hpp"

namespace editor {

namespace {

namespace fs = std::filesystem;
using engine::editor::CommandResult;

constexpr std::string_view start_scene = "main";

constexpr std::string_view main_script = R"(function on_start()
end

function on_player_joined(player)
end
)";

std::string folder_name(const fs::path& folder) {
    const fs::path absolute = fs::absolute(folder).lexically_normal();
    const fs::path name =
        absolute.has_filename() ? absolute.filename() : absolute.parent_path().filename();
    return name.string();
}

bool write_text(const fs::path& file, std::string_view text) {
    std::ofstream stream(file, std::ios::binary);
    stream << text;
    return static_cast<bool>(stream);
}

std::string project_json(const fs::path& folder) {
    const nlohmann::ordered_json project{{"name", folder_name(folder)},
                                         {"start_scene", start_scene}};
    return project.dump(4) + "\n";
}

std::string prefab_script(std::string_view name) {
    return std::format("entity \"{}\"\n"
                       "\n"
                       "color  = {{ r = 255, g = 255, b = 255, a = 255 }}\n"
                       "hitbox = {{ w = 64, h = 64 }}\n",
                       name);
}

std::optional<CommandResult> refuse_folder(const fs::path& folder) {
    std::error_code error;
    if (!fs::exists(folder, error)) {
        return std::nullopt;
    }
    if (!fs::is_directory(folder, error)) {
        return CommandResult::refused(std::format("{} is a file", folder.string()));
    }
    if (!fs::is_empty(folder, error) || error) {
        return CommandResult::refused(std::format(
            "{} is not empty; a new game needs an empty or missing folder", folder.string()));
    }
    return std::nullopt;
}

} // namespace

CommandResult create_project(const fs::path& folder) {
    if (auto refused = refuse_folder(folder)) {
        return *refused;
    }
    std::error_code error;
    fs::create_directories(folder / "scripts", error);
    fs::create_directories(folder / "assets", error);
    const bool written =
        !error && write_text(folder / "game.json", project_json(folder)) &&
        write_text(engine::script::scripts_folder(folder) / "main.lua", main_script) &&
        engine::script::write_scene(engine::script::scene_file(folder, start_scene), {});
    if (!written) {
        return CommandResult::refused(std::format("cannot create the game in {}", folder.string()));
    }
    return CommandResult::done(std::format("created a new game in {}", folder.string()));
}

fs::path prefab_file(const fs::path& game, std::string_view name) {
    return engine::script::scripts_folder(game) / (std::string(name) + ".lua");
}

CommandResult create_prefab(const fs::path& game, std::string_view name) {
    if (!engine::script::is_scene_name(name)) {
        return CommandResult::refused(
            std::format("'{}' is not a prefab name: use 1 to 63 letters, digits, _ or -", name));
    }
    const fs::path file = prefab_file(game, name);
    std::error_code error;
    if (fs::exists(file, error)) {
        return CommandResult::refused(std::format("{} already exists", file.string()));
    }
    fs::create_directories(file.parent_path(), error);
    if (error || !write_text(file, prefab_script(name))) {
        return CommandResult::refused(std::format("cannot write {}", file.string()));
    }
    return CommandResult::done(std::format("created {}", file.string()));
}

} // namespace editor
