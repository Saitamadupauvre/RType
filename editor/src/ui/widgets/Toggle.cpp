#include "ui/widgets/Toggle.hpp"

#include "ui/Text.hpp"

namespace editor {

namespace {

Clay_Color background(bool on, bool hovered) {
    if (on) {
        return color::selected;
    }
    return hovered ? color::hover : Clay_Color{};
}

} // namespace

bool toggle(Ui& ui, Clay_ElementId id, Clay_String label, bool on) {
    const Theme& theme = ui.theme();
    CLAY({.id = id,
          .layout = {.sizing = {.height = CLAY_SIZING_FIXED(theme.px(size::tool_button))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = background(on, Ui::hovered(id)),
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius))}) {
        text(theme, label, size::text, on ? color::focus : color::text_soft);
    }
    return ui.clicked(id);
}

} // namespace editor
