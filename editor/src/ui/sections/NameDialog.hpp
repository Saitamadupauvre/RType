#pragma once

#include <clay.h>

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Draws the dialog naming a new prefab or scene over the window while it is open; does
 * nothing otherwise.
 *
 * Its field keeps the keyboard. Enter or Create emits command::CreatePrefab or
 * command::CreateScene with the typed name; Esc, Cancel or a click outside closes it.
 */
void name_dialog(Ui& ui, const Editor& editor, Clay_Dimensions window);

} // namespace editor
