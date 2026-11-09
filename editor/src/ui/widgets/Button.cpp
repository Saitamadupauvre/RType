#include "ui/widgets/Button.hpp"

#include "ui/Text.hpp"

namespace editor {

namespace {

ButtonStyle resolve(ButtonStyle style, bool enabled, bool hovered) {
    if (!enabled) {
        return {.background = color::toolbar, .foreground = color::text_dim};
    }
    if (hovered) {
        return {.background = color::hover, .foreground = style.foreground};
    }
    return style;
}

void draw(const Theme& theme, Clay_ElementId id, Clay_String label, ButtonStyle shown) {
    CLAY({.id = id,
          .layout = {.sizing = {.height = CLAY_SIZING_FIXED(theme.px(size::button))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = shown.background,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        text(theme, label, size::text, shown.foreground);
    }
}

} // namespace

bool button(Ui& ui, Clay_ElementId id, Clay_String label, ButtonStyle style, bool enabled) {
    draw(ui.theme(), id, label, resolve(style, enabled, Ui::hovered(id)));
    return enabled && ui.clicked(id);
}

} // namespace editor
