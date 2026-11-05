#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

struct ButtonStyle {
    Clay_Color background;
    Clay_Color foreground;
};

[[nodiscard]] bool button(Ui& ui, Clay_ElementId id, Clay_String label, ButtonStyle style,
                          bool enabled = true);

void disabled_button(const Theme& theme, Clay_String label);

} // namespace editor
