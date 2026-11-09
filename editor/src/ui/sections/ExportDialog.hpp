#pragma once

#include <clay.h>

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/** @brief Draws the export dialog over the window while it is open; does nothing otherwise. */
void export_dialog(Ui& ui, const Editor& editor, Clay_Dimensions window);

} // namespace editor
