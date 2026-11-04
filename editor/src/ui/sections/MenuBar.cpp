#include "ui/sections/MenuBar.hpp"

#include <clay.h>

#include "ui/Text.hpp"

namespace editor {

namespace {

void menu_item(const Theme& theme, Clay_String label) {
    text(theme, label, size::text, color::text_soft);
}

void button(const Theme& theme, Clay_String label, Clay_Color background, Clay_Color foreground) {
    CLAY({.layout = {.sizing = {.height = CLAY_SIZING_FIXED(theme.px(size::button))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = background,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        text(theme, label, size::text, foreground);
    }
}

} // namespace

void menu_bar(const Theme& theme) {
    CLAY({.id = CLAY_ID("MenuBar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::menu_bar))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(14.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::menu_bar,
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        text(theme, CLAY_STRING("engine editor"), size::text, color::text, Face::bold);
        menu_item(theme, CLAY_STRING("File"));
        menu_item(theme, CLAY_STRING("Edit"));
        menu_item(theme, CLAY_STRING("View"));
        menu_item(theme, CLAY_STRING("Scene"));
        menu_item(theme, CLAY_STRING("Help"));
        spacer();
        text(theme, CLAY_STRING("no scene open"), size::small_text, color::text_muted);
        spacer();
        button(theme, CLAY_STRING("Play"), color::accent, color::text_on_accent);
        button(theme, CLAY_STRING("Stop"), color::toolbar, color::text_dim);
        button(theme, CLAY_STRING("Export..."), color::toolbar, color::text);
    }
}

} // namespace editor
