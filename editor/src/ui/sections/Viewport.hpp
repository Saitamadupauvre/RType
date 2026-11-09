#pragma once

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Draws the open scene through the editor camera and turns clicks and drags into Select
 * and Move. The scene itself is painted by the renderer from the frame's ViewportFrame.
 */
void viewport(Ui& ui, const Editor& editor);

} // namespace editor
