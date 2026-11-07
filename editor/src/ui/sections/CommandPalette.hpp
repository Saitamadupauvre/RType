#pragma once

#include <clay.h>

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Opens the command palette on its shortcut and draws it while it is open.
 *
 * The palette lists the command table filtered by what is typed, best match first. Up and Down
 * move the highlight, Enter or a click runs an enabled entry, Esc or a click outside closes it.
 * Its text field holds the keyboard, so shortcuts are ignored while it is open.
 */
void command_palette(Ui& ui, const Editor& editor, Clay_Dimensions window);

} // namespace editor
