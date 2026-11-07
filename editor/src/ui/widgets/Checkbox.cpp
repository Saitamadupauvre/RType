#include "ui/widgets/Checkbox.hpp"

#include "ui/Text.hpp"

namespace editor {

namespace {

constexpr float box_size = 14.F;
constexpr float mark_size = 8.F;

void mark(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(mark_size)),
                                .height = CLAY_SIZING_FIXED(theme.px(mark_size))}},
          .backgroundColor = color::text_on_accent,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(2.F))}) {}
}

void box(const Theme& theme, bool value, bool hovered) {
    const Clay_Color border = hovered ? color::focus : color::border;
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(box_size)),
                                .height = CLAY_SIZING_FIXED(theme.px(box_size))},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = value ? color::accent : color::field,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(3.F)),
          .border = {.color = value ? color::accent : border,
                     .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        if (value) {
            mark(theme);
        }
    }
}

} // namespace

bool checkbox(Ui& ui, Clay_ElementId id, bool value, Clay_String label) {
    const Theme& theme = ui.theme();
    CLAY({.id = id,
          .layout = {.sizing = {.height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .childGap = theme.px16(8.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        box(theme, value, Ui::hovered(id));
        text(theme, label, size::text, color::text_soft);
    }
    return ui.clicked(id);
}

} // namespace editor
