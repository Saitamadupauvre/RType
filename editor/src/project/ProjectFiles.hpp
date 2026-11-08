#pragma once

#include <filesystem>
#include <string_view>

#include "engine/editor/CommandResult.hpp"

namespace editor {

/**
 * @brief Writes a new, empty game folder at @p folder: `game.json` (named after the folder,
 * starting with scene `main`), `scenes/main.json`, `scripts/main.lua` and `assets/`.
 *
 * Refused, writing nothing, when @p folder is a file or a folder that is not empty. Missing
 * parent folders are created.
 */
[[nodiscard]] engine::editor::CommandResult create_project(const std::filesystem::path& folder);

/**
 * @brief Writes `scripts/<name>.lua` in @p game, declaring entity @p name drawn as a white
 * 64 px square with a matching hitbox.
 *
 * Refused for a name that is not 1 to 63 letters, digits, `_` or `-`, or when the file exists.
 */
[[nodiscard]] engine::editor::CommandResult create_prefab(const std::filesystem::path& game,
                                                          std::string_view name);

/** @return `<game>/scripts/<name>.lua`, the file create_prefab() writes. */
[[nodiscard]] std::filesystem::path prefab_file(const std::filesystem::path& game,
                                                std::string_view name);

} // namespace editor
