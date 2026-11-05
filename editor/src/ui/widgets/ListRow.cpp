#include "ui/widgets/ListRow.hpp"

#include "ui/Text.hpp"

namespace editor {

namespace {

Clay_Color background(const RowContent& content, bool hovered) {
    if (content.selected) {
        return color::selected;
    }
    return hovered ? color::hover : Clay_Color{};
}

void row(const Theme& theme, Clay_ElementId id, const RowContent& content, bool hovered) {
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(8.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = background(content, hovered)}) {
        text(theme, content.label, size::text, content.selected ? color::text : color::text_soft);
        spacer();
        if (content.detail.length > 0) {
            text(theme, content.detail, size::small_text, color::text_muted);
        }
    }
}

} // namespace

void list_item(const Theme& theme, const RowContent& content) {
    row(theme, Clay_ElementId{}, content, false);
}

bool list_row(Ui& ui, Clay_ElementId id, const RowContent& content) {
    row(ui.theme(), id, content, Ui::hovered(id));
    return ui.clicked(id);
}

void empty_hint(const Theme& theme, Clay_String hint) {
    CLAY({.layout = {
              .sizing = {.width = CLAY_SIZING_GROW(),
                         .height = CLAY_SIZING_FIXED(theme.px(size::row))},
              .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)},
              .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        text(theme, hint, size::small_text, color::text_dim);
    }
}

} // namespace editor
