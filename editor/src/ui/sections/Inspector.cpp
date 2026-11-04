#include "ui/sections/Inspector.hpp"

#include <clay.h>

#include "ui/Text.hpp"
#include "ui/sections/PanelHeader.hpp"

namespace editor {

void inspector(const Theme& theme) {
    CLAY({.id = CLAY_ID("Inspector"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::inspector)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.left = theme.px16(1.F)}}}) {
        panel_header(theme, CLAY_STRING("INSPECTOR"));
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}) {
            text(theme, CLAY_STRING("Nothing selected"), size::text, color::text_muted);
        }
    }
}

} // namespace editor
