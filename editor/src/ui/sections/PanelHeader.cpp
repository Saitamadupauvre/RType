#include "ui/sections/PanelHeader.hpp"

#include "ui/Text.hpp"

namespace editor {

void panel_header(const Theme& theme, Clay_String title) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::panel_header))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        text(theme, title, size::small_text, color::text_muted);
    }
}

} // namespace editor
