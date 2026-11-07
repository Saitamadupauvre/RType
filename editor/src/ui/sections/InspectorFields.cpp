#include "ui/sections/InspectorFields.hpp"

#include <clay.h>

#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <variant>

#include "action/Overloaded.hpp"
#include "ui/FieldText.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/Checkbox.hpp"
#include "ui/widgets/NumberField.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

using engine::core::FieldValue;

constexpr float label_width = 120.F;
constexpr float swatch_size = 16.F;

Clay_ElementId value_id(const FieldRow& row) { return CLAY_IDI("InspectorValue", row.index); }

Clay_ElementId second_value_id(const FieldRow& row) {
    return CLAY_IDI("InspectorValueY", row.index);
}

void set(Ui& ui, const FieldRow& row, FieldValue value) {
    if (value != row.field.value) {
        ui.emit(edit::SetField{.id = row.id, .key = row.field.key, .value = std::move(value)});
    }
}

void reject(Ui& ui, const FieldRow& row, std::string_view expected) {
    ui.emit(command::ReportInvalidInput{
        .message = std::format("{}: expected {}", row.field.key, expected)});
}

template <typename Number>
void number(Ui& ui, const FieldRow& row, Clay_ElementId id, Number value, auto make) {
    const FieldResult<Number> result = number_field(ui, id, value);
    if (result.event == FieldEvent::committed) {
        set(ui, row, make(result.value));
    } else if (result.event == FieldEvent::rejected) {
        reject(ui, row, "a number");
    }
}

void vec2(Ui& ui, const FieldRow& row, engine::core::Vec2 value) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(4.F)}}) {
        number(ui, row, value_id(row), value.x,
               [&](float x) { return FieldValue{engine::core::Vec2{x, value.y}}; });
        number(ui, row, second_value_id(row), value.y,
               [&](float y) { return FieldValue{engine::core::Vec2{value.x, y}}; });
    }
}

void swatch(const Theme& theme, engine::core::Rgba color) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(swatch_size)),
                                .height = CLAY_SIZING_FIXED(theme.px(swatch_size))}},
          .backgroundColor = {.r = static_cast<float>(color.r),
                              .g = static_cast<float>(color.g),
                              .b = static_cast<float>(color.b),
                              .a = static_cast<float>(color.a)},
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(3.F)),
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {}
}

void hex_field(Ui& ui, const FieldRow& row, engine::core::Rgba value) {
    const Clay_String hex = ui.keep(color_to_hex(value));
    const TextResult result =
        text_field(ui, value_id(row), {hex.chars, static_cast<std::size_t>(hex.length)});
    if (result.event != FieldEvent::committed) {
        return;
    }
    if (const auto color = color_from_hex(result.value)) {
        set(ui, row, FieldValue{*color});
    } else {
        reject(ui, row, "a color such as #FF8000 or #FF8000FF");
    }
}

void color_value(Ui& ui, const FieldRow& row, engine::core::Rgba value) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(6.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        swatch(ui.theme(), value);
        hex_field(ui, row, value);
    }
}

void text_value(Ui& ui, const FieldRow& row, const std::string& value) {
    const TextResult result = text_field(ui, value_id(row), value);
    if (result.event == FieldEvent::committed) {
        set(ui, row, FieldValue{result.value});
    }
}

void read_only(Ui& ui, const FieldRow& row) {
    std::string shown = describe(row.field.value);
    text(ui.theme(), shown.empty() ? CLAY_STRING("none") : ui.keep(std::move(shown)), size::text,
         color::text_muted);
}

void value(Ui& ui, const FieldRow& row) {
    std::visit(Overloaded{
                   [&](float number_value) {
                       number(ui, row, value_id(row), number_value,
                              [](float result) { return FieldValue{result}; });
                   },
                   [&](std::int64_t number_value) {
                       number(ui, row, value_id(row), number_value,
                              [](std::int64_t result) { return FieldValue{result}; });
                   },
                   [&](bool flag) {
                       if (checkbox(ui, value_id(row), flag, CLAY_STRING(""))) {
                           set(ui, row, FieldValue{!flag});
                       }
                   },
                   [&](const std::string& text_content) { text_value(ui, row, text_content); },
                   [&](engine::core::Vec2 vector) { vec2(ui, row, vector); },
                   [&](engine::core::Rgba color) { color_value(ui, row, color); },
                   [&](const engine::core::AssetName&) { read_only(ui, row); },
                   [&](const engine::core::FieldList&) { read_only(ui, row); },
               },
               row.field.value);
}

void label(Ui& ui, const FieldRow& row) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(label_width))}}}) {
        text(theme, view_of(row.label), size::small_text,
             row.field.overridden ? color::accent : color::text_muted);
    }
}

void reset_button(Ui& ui, const FieldRow& row) {
    if (!row.field.overridden) {
        return;
    }
    if (button(ui, CLAY_IDI("InspectorReset", row.index), CLAY_STRING("Reset"),
               {.background = color::field, .foreground = color::text_soft})) {
        ui.emit(edit::ResetField{.id = row.id, .key = row.field.key});
    }
}

} // namespace

void field_row(Ui& ui, const FieldRow& row) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIT(theme.px(size::row), 0)},
                     .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(6.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        label(ui, row);
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}) { value(ui, row); }
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(reset_width))}}}) {
            reset_button(ui, row);
        }
    }
}

} // namespace editor
