#include "ui/sections/StatusBar.hpp"

#include <clay.h>

#include "ui/Text.hpp"

namespace editor {

void status_bar(const Theme& theme) {
    CLAY({.id = CLAY_ID("StatusBar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::status_bar))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(18.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::menu_bar,
          .border = {.color = color::border, .width = {.top = theme.px16(1.F)}}}) {
        text(theme, CLAY_STRING("edit mode"), size::small_text, color::text_muted);
        spacer();
        text(theme, CLAY_STRING("no scene open"), size::small_text, color::text_muted);
    }
}

} // namespace editor
