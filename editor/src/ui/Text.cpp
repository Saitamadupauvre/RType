#include "ui/Text.hpp"

#include <cstdint>

namespace editor {

void text(const Theme& theme, Clay_String content, float size, Clay_Color color, Face face) {
    CLAY_TEXT(content, CLAY_TEXT_CONFIG({.textColor = color,
                                         .fontId = static_cast<std::uint16_t>(face),
                                         .fontSize = theme.px16(size),
                                         .wrapMode = CLAY_TEXT_WRAP_NONE}));
}

void wrapped_text(const Theme& theme, Clay_String content, float size, Clay_Color color) {
    CLAY_TEXT(content, CLAY_TEXT_CONFIG({.textColor = color,
                                         .fontId = static_cast<std::uint16_t>(Face::regular),
                                         .fontSize = theme.px16(size),
                                         .wrapMode = CLAY_TEXT_WRAP_WORDS}));
}

void spacer() {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {}
}

Clay_String view_of(std::string_view stable) {
    return {.isStaticallyAllocated = false,
            .length = static_cast<std::int32_t>(stable.size()),
            .chars = stable.data()};
}

} // namespace editor
