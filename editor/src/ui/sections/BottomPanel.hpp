#pragma once

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief The console height for a window `window_height` pixels tall: the design height, shrunk
 * down to its minimum when the workspace would get less than its own minimum. It depends on the
 * window only, never on what the panels show.
 */
[[nodiscard]] float bottom_panel_height(const Theme& theme, float window_height);

void bottom_panel(Ui& ui, const Editor& editor, float height);

} // namespace editor
