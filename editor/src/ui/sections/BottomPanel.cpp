#include "ui/sections/BottomPanel.hpp"

#include <clay.h>

#include <cstdint>

#include "ui/Text.hpp"

namespace editor {

namespace {

void tab(const Theme& theme, Clay_String label, bool active) {
    CLAY({.layout = {.sizing = {.height = CLAY_SIZING_GROW()},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.color = color::focus,
                     .width = {.bottom = active ? theme.px16(2.F) : std::uint16_t{0}}}}) {
        text(theme, label, size::text, active ? color::text : color::text_muted);
    }
}

} // namespace

void bottom_panel(const Theme& theme) {
    CLAY({.id = CLAY_ID("BottomPanel"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::bottom_panel))},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::toolbar,
          .border = {.color = color::border, .width = {.top = theme.px16(1.F)}}}) {
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                    .height = CLAY_SIZING_FIXED(theme.px(size::tabs))},
                         .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)}},
              .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
            tab(theme, CLAY_STRING("Console"), true);
            tab(theme, CLAY_STRING("Problems"), false);
        }
        CLAY({.id = CLAY_ID("Console"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {}
    }
}

} // namespace editor
