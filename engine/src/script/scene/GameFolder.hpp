#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace engine::script {

struct GameProject {
    std::optional<std::string> start_scene;
};

[[nodiscard]] bool is_scene_name(std::string_view name) noexcept;

[[nodiscard]] std::filesystem::path scene_file(const std::filesystem::path& game,
                                               std::string_view name);

[[nodiscard]] std::filesystem::path scripts_folder(const std::filesystem::path& game);

[[nodiscard]] std::optional<GameProject> read_project(const std::filesystem::path& game);

} // namespace engine::script
