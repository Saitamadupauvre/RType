#include "ui/widgets/Dropdown.hpp"

#include <algorithm>
#include <cstdint>

#include "ui/Text.hpp"

namespace editor {

namespace {

constexpr float padding = 6.F;
constexpr std::int16_t popup_layer = 100;
constexpr std::string_view arrow = "\xE2\x96\xBE";

Clay_ElementId list_id() { return CLAY_ID("DropdownList"); }

Clay_ElementId option_id(std::size_t index) {
    return CLAY_IDI("DropdownOption", static_cast<std::uint32_t>(index));
}

std::optional<std::size_t> clicked_option(const Ui& ui, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (ui.clicked(option_id(index))) {
            return index;
        }
    }
    return std::nullopt;
}

void move_highlight(const UiInput& input, std::size_t count, std::size_t& highlighted) {
    if (pressed_or_repeated(input, Key::up) && highlighted > 0) {
        --highlighted;
    }
    if (pressed_or_repeated(input, Key::down) && highlighted + 1 < count) {
        ++highlighted;
    }
}

std::optional<std::size_t> handle_open(Ui& ui, std::size_t count) {
    WidgetState& widgets = ui.widgets();
    const UiInput& input = ui.input();
    if (const std::optional<std::size_t> picked = clicked_option(ui, count)) {
        widgets.close();
        return picked;
    }
    if ((input.primary_pressed && !Ui::hovered(list_id())) ||
        pressed_or_repeated(input, Key::escape)) {
        widgets.close();
        return std::nullopt;
    }
    if (pressed_or_repeated(input, Key::enter)) {
        widgets.close();
        return widgets.highlighted();
    }
    move_highlight(input, count, widgets.highlighted());
    return std::nullopt;
}

Clay_Color option_background(bool highlighted, bool hovered) {
    if (hovered) {
        return color::hover;
    }
    return highlighted ? color::selected : Clay_Color{};
}

void option_row(const Theme& theme, std::size_t index, std::string_view label, bool highlighted) {
    const Clay_ElementId id = option_id(index);
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(padding), .right = theme.px16(padding)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = option_background(highlighted, Ui::hovered(id))}) {
        text(theme, view_of(label), size::text, color::text);
    }
}

void option_list(Ui& ui, Clay_ElementId id, std::span<const std::string_view> options) {
    const Theme& theme = ui.theme();
    const float width = Clay_GetElementData(id).boundingBox.width;
    const std::size_t highlighted = ui.widgets().highlighted();
    CLAY({.id = list_id(),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(width)},
                     .padding = CLAY_PADDING_ALL(theme.px16(2.F)),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .floating = {.offset = {.y = theme.px(2.F)},
                       .zIndex = popup_layer,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_PARENT,
                       .clipTo = CLAY_CLIP_TO_NONE},
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        for (std::size_t index = 0; index < options.size(); ++index) {
            option_row(theme, index, options[index], index == highlighted);
        }
    }
}

void draw(Ui& ui, Clay_ElementId id, std::string_view label,
          std::span<const std::string_view> options) {
    const Theme& theme = ui.theme();
    const bool open = ui.widgets().is_open(id);
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(padding), .right = theme.px16(padding)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = Ui::hovered(id) ? color::hover : color::field,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .border = {.color = open ? color::focus : color::border,
                     .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        text(theme, view_of(label), size::text, color::text);
        spacer();
        text(theme, view_of(arrow), size::text, color::text_muted);
        if (open) {
            option_list(ui, id, options);
        }
    }
}

} // namespace

std::optional<std::size_t> dropdown(Ui& ui, Clay_ElementId id, std::size_t selected,
                                    std::span<const std::string_view> options) {
    std::optional<std::size_t> picked;
    if (ui.widgets().is_open(id)) {
        picked = handle_open(ui, options.size());
    } else if (ui.clicked(id) && !options.empty()) {
        ui.widgets().open(id, std::min(selected, options.size() - 1));
    }
    const std::string_view label = selected < options.size() ? options[selected] : "";
    draw(ui, id, label, options);
    return picked;
}

} // namespace editor
