#include "GameFolder.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <system_error>

#include "JsonFile.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

constexpr std::string_view project_file = "game.json";
constexpr std::size_t max_scene_name_length = 63;

bool is_scene_name_character(char character) noexcept {
    const bool lower = character >= 'a' && character <= 'z';
    const bool upper = character >= 'A' && character <= 'Z';
    const bool digit = character >= '0' && character <= '9';
    return lower || upper || digit || character == '_' || character == '-';
}

std::optional<std::string> start_scene_of(const Json& project, const fs::path& file) {
    if (!project.is_object()) {
        throw std::runtime_error(std::format("{} must hold a JSON object", file.generic_string()));
    }
    const auto found = project.find("start_scene");
    if (found == project.end()) {
        return std::nullopt;
    }
    if (!found->is_string() || !is_scene_name(found->get<std::string>())) {
        throw std::runtime_error(std::format(
            "{}: start_scene must be a scene name of 1 to {} letters, digits, '_' or '-'",
            file.generic_string(), max_scene_name_length));
    }
    return found->get<std::string>();
}

} // namespace

bool is_scene_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= max_scene_name_length &&
           std::ranges::all_of(name, is_scene_name_character);
}

fs::path scene_file(const fs::path& game, std::string_view name) {
    return game / "scenes" / (std::string(name) + ".json");
}

fs::path scripts_folder(const fs::path& game) { return game / "scripts"; }

std::optional<GameProject> read_project(const fs::path& game) {
    const fs::path file = game / project_file;
    std::error_code error;
    if (!fs::exists(file, error)) {
        return GameProject{};
    }
    try {
        return GameProject{.start_scene = start_scene_of(read_json_file(file), file)};
    } catch (const std::runtime_error& invalid) {
        log::error("game: {}", invalid.what());
    }
    return std::nullopt;
}

} // namespace engine::script
