#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Draws a box, checked when `value`, followed by `label`.
 * @return Whether it was clicked this frame; the caller emits `!value`.
 */
[[nodiscard]] bool checkbox(Ui& ui, Clay_ElementId id, bool value, Clay_String label);

} // namespace editor
