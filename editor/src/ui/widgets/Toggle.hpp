#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Draws an on/off label, highlighted when `on`.
 * @return Whether it was clicked this frame; the caller emits the new value.
 */
[[nodiscard]] bool toggle(Ui& ui, Clay_ElementId id, Clay_String label, bool on);

} // namespace editor
