#pragma once

#include "action/Editor.hpp"
#include "settings/Settings.hpp"

namespace editor {

[[nodiscard]] bool run_editor_window(Editor& editor, const Settings& settings);

} // namespace editor
