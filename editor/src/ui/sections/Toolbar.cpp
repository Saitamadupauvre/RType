#include "ui/sections/Toolbar.hpp"

#include <clay.h>

#include "ui/Text.hpp"

namespace editor {

namespace {

void tool_button(const Theme& theme, Clay_String key, bool active) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::tool_button)),
                                .height = CLAY_SIZING_FIXED(theme.px(size::tool_button))},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = active ? color::selected : Clay_Color{},
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius))}) {
        text(theme, key, size::text, active ? color::focus : color::text_soft);
    }
}

void separator(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(1.F)),
                                .height = CLAY_SIZING_FIXED(theme.px(size::separator))}},
          .backgroundColor = color::border}) {}
}

} // namespace

void toolbar(const Theme& theme) {
    CLAY({.id = CLAY_ID("Toolbar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::toolbar))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)},
                     .childGap = theme.px16(8.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::toolbar,
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        tool_button(theme, CLAY_STRING("V"), true);
        tool_button(theme, CLAY_STRING("G"), false);
        tool_button(theme, CLAY_STRING("H"), false);
        separator(theme);
        text(theme, CLAY_STRING("Snap"), size::text, color::text_soft);
        text(theme, CLAY_STRING("Grid 16 px"), size::text, color::text_muted);
        separator(theme);
        text(theme, CLAY_STRING("Hitboxes"), size::text, color::text_soft);
        text(theme, CLAY_STRING("Names"), size::text, color::text_soft);
        spacer();
        text(theme, CLAY_STRING("Undo"), size::text, color::text_soft);
        text(theme, CLAY_STRING("Redo"), size::text, color::text_soft);
    }
}

} // namespace editor
