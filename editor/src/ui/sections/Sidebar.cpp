#include "ui/sections/Sidebar.hpp"

#include <clay.h>

#include "ui/sections/PanelHeader.hpp"

namespace editor {

namespace {

void panel_section(const Theme& theme, Clay_ElementId id, Clay_String title) {
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        panel_header(theme, title);
    }
}

} // namespace

void sidebar(const Theme& theme) {
    CLAY({.id = CLAY_ID("Sidebar"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::sidebar)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.right = theme.px16(1.F)}}}) {
        panel_section(theme, CLAY_ID("Prefabs"), CLAY_STRING("PREFABS"));
        panel_section(theme, CLAY_ID("Scenes"), CLAY_STRING("SCENES"));
        panel_section(theme, CLAY_ID("Instances"), CLAY_STRING("INSTANCES"));
    }
}

} // namespace editor
