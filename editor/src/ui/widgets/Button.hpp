#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

struct ButtonStyle {
    Clay_Color background;
    Clay_Color foreground;
};

/**
 * @brief Draws a button.
 * @return Whether it was clicked this frame; always false when disabled.
 */
[[nodiscard]] bool button(Ui& ui, Clay_ElementId id, Clay_String label, ButtonStyle style,
                          bool enabled = true);


} // namespace editor
