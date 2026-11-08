#include "ui/sections/InspectorFields.hpp"

#include <clay.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "action/Overloaded.hpp"
#include "ui/FieldText.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/Checkbox.hpp"
#include "ui/widgets/NumberField.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

using engine::core::FieldKind;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::ScalarValue;

constexpr float label_width = 120.F;
constexpr float swatch_size = 16.F;
constexpr std::uint32_t items_per_row = 1000;

struct ValueSlot {
    Clay_ElementId id;
    Clay_ElementId second_id;
    std::function<void(FieldValue)> commit;
    std::function<void(std::string_view expected)> reject;
};

template <typename Number>
void number(Ui& ui, const ValueSlot& slot, Clay_ElementId id, Number value, auto make) {
    const FieldResult<Number> result = number_field(ui, id, value);
    if (result.event == FieldEvent::committed) {
        slot.commit(make(result.value));
    } else if (result.event == FieldEvent::rejected) {
        slot.reject("a number");
    }
}

void vec2(Ui& ui, const ValueSlot& slot, engine::core::Vec2 value) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(4.F)}}) {
        number(ui, slot, slot.id, value.x,
               [&](float x) { return FieldValue{engine::core::Vec2{x, value.y}}; });
        number(ui, slot, slot.second_id, value.y,
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

void hex_field(Ui& ui, const ValueSlot& slot, engine::core::Rgba value) {
    const Clay_String hex = ui.keep(color_to_hex(value));
    const TextResult result =
        text_field(ui, slot.id, {hex.chars, static_cast<std::size_t>(hex.length)});
    if (result.event != FieldEvent::committed) {
        return;
    }
    if (const auto color = color_from_hex(result.value)) {
        slot.commit(FieldValue{*color});
    } else {
        slot.reject("a color such as #FF8000 or #FF8000FF");
    }
}

void color_value(Ui& ui, const ValueSlot& slot, engine::core::Rgba value) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(6.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        swatch(ui.theme(), value);
        hex_field(ui, slot, value);
    }
}

void text_value(Ui& ui, const ValueSlot& slot, std::string_view value, auto make) {
    const Clay_String kept = ui.keep(std::string(value));
    const TextResult result = text_field(
        ui, slot.id, {kept.chars, static_cast<std::size_t>(kept.length)}, {.placeholder = "none"});
    if (result.event == FieldEvent::committed) {
        slot.commit(make(result.value));
    }
}

void scalar_value(Ui& ui, const ValueSlot& slot, const FieldValue& value) {
    std::visit(Overloaded{
                   [&](float number_value) {
                       number(ui, slot, slot.id, number_value,
                              [](float result) { return FieldValue{result}; });
                   },
                   [&](std::int64_t number_value) {
                       number(ui, slot, slot.id, number_value,
                              [](std::int64_t result) { return FieldValue{result}; });
                   },
                   [&](bool flag) {
                       if (checkbox(ui, slot.id, flag, CLAY_STRING(""))) {
                           slot.commit(FieldValue{!flag});
                       }
                   },
                   [&](const std::string& content) {
                       text_value(ui, slot, content,
                                  [](const std::string& typed) { return FieldValue{typed}; });
                   },
                   [&](engine::core::Vec2 vector) { vec2(ui, slot, vector); },
                   [&](engine::core::Rgba color) { color_value(ui, slot, color); },
                   [&](const engine::core::AssetName& asset) {
                       text_value(ui, slot, asset.path, [](const std::string& typed) {
                           return FieldValue{engine::core::AssetName{.path = typed}};
                       });
                   },
                   [&](const FieldList&) {},
               },
               value);
}

FieldValue widen(const ScalarValue& item) {
    return std::visit([](const auto& known) { return FieldValue{known}; }, item);
}

ScalarValue narrow(const FieldValue& value) {
    return std::visit(Overloaded{
                          [](const FieldList&) { return ScalarValue{std::string{}}; },
                          [](const auto& known) { return ScalarValue{known}; },
                      },
                      value);
}

ScalarValue default_item(FieldKind kind) {
    switch (kind) {
    case FieldKind::Float:
        return 0.F;
    case FieldKind::Int:
        return std::int64_t{0};
    case FieldKind::Bool:
        return false;
    case FieldKind::Vec2:
        return engine::core::Vec2{0.F, 0.F};
    case FieldKind::Color:
        return engine::core::Rgba{};
    case FieldKind::Asset:
        return engine::core::AssetName{};
    case FieldKind::String:
    case FieldKind::List:
        break;
    }
    return std::string{};
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

ValueSlot slot_of(Ui& ui, const FieldRow& row) {
    return {.id = CLAY_IDI("InspectorValue", row.index),
            .second_id = CLAY_IDI("InspectorValueY", row.index),
            .commit = [&ui, &row](FieldValue value) { set(ui, row, std::move(value)); },
            .reject = [&ui, &row](std::string_view expected) { reject(ui, row, expected); }};
}

ButtonStyle small_button() { return {.background = color::field, .foreground = color::text_soft}; }

void list_item(Ui& ui, const FieldRow& row, const FieldList& list, std::size_t item) {
    const auto at = (row.index * items_per_row) + static_cast<std::uint32_t>(item);
    const auto replace = [&ui, &row, &list, item](const FieldValue& value) {
        FieldList changed = list;
        changed.items[item] = narrow(value);
        set(ui, row, FieldValue{std::move(changed)});
    };
    const ValueSlot slot{.id = CLAY_IDI("InspectorItem", at),
                         .second_id = CLAY_IDI("InspectorItemY", at),
                         .commit = replace,
                         .reject = [&ui, &row](std::string_view expected) {
                             reject(ui, row, expected);
                         }};
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(4.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}) {
            scalar_value(ui, slot, widen(list.items[item]));
        }
        if (button(ui, CLAY_IDI("InspectorItemRemove", at), CLAY_STRING("x"), small_button())) {
            FieldList changed = list;
            changed.items.erase(changed.items.begin() + static_cast<std::ptrdiff_t>(item));
            set(ui, row, FieldValue{std::move(changed)});
        }
    }
}

void list_value(Ui& ui, const FieldRow& row, const FieldList& list) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = ui.theme().px16(4.F),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        for (std::size_t item = 0; item < list.items.size(); ++item) {
            list_item(ui, row, list, item);
        }
        if (button(ui, CLAY_IDI("InspectorItemAdd", row.index), CLAY_STRING("+ Add"),
                   small_button())) {
            FieldList changed = list;
            changed.items.push_back(default_item(list.element));
            set(ui, row, FieldValue{std::move(changed)});
        }
    }
}

void value(Ui& ui, const FieldRow& row) {
    if (const auto* list = std::get_if<FieldList>(&row.field.value)) {
        list_value(ui, row, *list);
        return;
    }
    scalar_value(ui, slot_of(ui, row), row.field.value);
}

void label(Ui& ui, const FieldRow& row) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(label_width))}}}) {
        wrapped_text(theme, view_of(row.label), size::small_text,
                     row.field.overridden ? color::accent : color::text_muted);
    }
}

void reset_button(Ui& ui, const FieldRow& row) {
    if (!row.field.overridden) {
        return;
    }
    if (button(ui, CLAY_IDI("InspectorReset", row.index), CLAY_STRING("Reset"), small_button())) {
        ui.emit(edit::ResetField{.id = row.id, .key = row.field.key});
    }
}

} // namespace

void field_row(Ui& ui, const FieldRow& row) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIT(theme.px(size::row), 0)},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter),
                                 .top = theme.px16(row_padding),
                                 .bottom = theme.px16(row_padding)},
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
