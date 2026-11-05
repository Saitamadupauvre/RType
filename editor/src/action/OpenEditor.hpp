#pragma once

#include <filesystem>
#include <optional>

#include "action/Editor.hpp"

namespace editor {

[[nodiscard]] std::optional<Editor> open_editor(const std::filesystem::path& game);

} // namespace editor
