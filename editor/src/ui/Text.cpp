#include "ui/Text.hpp"

namespace editor {

void text(const Theme& theme, Clay_String content, float size, Clay_Color color, Face face) {
    CLAY_TEXT(content, CLAY_TEXT_CONFIG({.textColor = color,
                                         .fontId = static_cast<std::uint16_t>(face),
                                         .fontSize = theme.px16(size),
                                         .wrapMode = CLAY_TEXT_WRAP_NONE}));
}

void spacer() {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {}
}

} // namespace editor
