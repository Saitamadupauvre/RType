#include "ui/sections/Viewport.hpp"

#include <clay.h>

#include "ui/Theme.hpp"

namespace editor {

void viewport() {
    CLAY({.id = CLAY_ID("Viewport"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}},
          .backgroundColor = color::viewport}) {}
}

} // namespace editor
