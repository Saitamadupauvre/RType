#pragma once

#include <filesystem>
#include <optional>

#include "action/Editor.hpp"

namespace editor {

/**
 * @brief Opens a game folder and its start scene, if game.json names one.
 * @return std::nullopt when the session cannot open the folder. A start scene that fails to open
 * leaves no scene open and a line in the console.
 */
[[nodiscard]] std::optional<Editor> open_editor(const std::filesystem::path& game);

} // namespace editor
