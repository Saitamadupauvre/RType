#pragma once

#include <clay.h>

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/** @brief The theme for a frame: the screen's DPI scale times `ui.scale`, and the scroll speed. */
[[nodiscard]] Theme theme_for(const EditorState& state, float dpi_scale);

/**
 * @brief Lays out the whole window from the editor and emits the frame's actions into `ui`.
 * @return Render commands, valid until the next Clay layout.
 */
Clay_RenderCommandArray build_frame(Ui& ui, const Editor& editor, Clay_Dimensions window);

} // namespace editor
