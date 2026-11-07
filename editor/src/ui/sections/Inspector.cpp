#include "ui/sections/Inspector.hpp"

#include <clay.h>

#include <cstdint>
#include <format>
#include <utility>
#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

#include "ui/FieldText.hpp"
#include "ui/Labels.hpp"
#include "ui/Text.hpp"
#include "ui/sections/InspectorFields.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/ListRow.hpp"
#include "ui/widgets/NumberField.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

constexpr float label_width = 120.F;
constexpr float component_gap = 8.F;

void nothing_selected(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}) {
        text(theme, CLAY_STRING("Nothing selected"), size::text, color::text_muted);
    }
}

void row(Ui& ui, Clay_String name, auto&& content) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIT(theme.px(size::row), 0)},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter),
                                 .top = theme.px16(row_padding),
                                 .bottom = theme.px16(row_padding)},
                     .childGap = theme.px16(6.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(label_width))}}}) {
            text(theme, name, size::small_text, color::text_muted);
        }
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                         .childGap = theme.px16(4.F)}}) {
            content();
        }
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(reset_width))}}}) {}
    }
}

void name_row(Ui& ui, const engine::script::Instance& instance, InstanceId id) {
    row(ui, CLAY_STRING("Name"), [&] {
        const TextResult result = text_field(ui, CLAY_ID("InspectorName"), instance.name);
        if (result.event == FieldEvent::committed && result.value != instance.name) {
            ui.emit(edit::Rename{.id = id, .name = result.value});
        }
    });
}

void coordinate(Ui& ui, Clay_ElementId field_id, float value, InstanceId id, auto place) {
    const FieldResult<float> result = number_field(ui, field_id, value);
    if (result.event == FieldEvent::committed && result.value != value) {
        ui.emit(edit::Move{.id = id, .position = place(result.value)});
    } else if (result.event == FieldEvent::rejected) {
        ui.emit(command::ReportInvalidInput{.message = "Position: expected a number"});
    }
}

void position_row(Ui& ui, engine::core::Vec2 position, InstanceId id) {
    row(ui, CLAY_STRING("Position"), [&] {
        coordinate(ui, CLAY_ID("InspectorPositionX"), position.x, id,
                   [&](float x) { return engine::core::Vec2{x, position.y}; });
        coordinate(ui, CLAY_ID("InspectorPositionY"), position.y, id,
                   [&](float y) { return engine::core::Vec2{position.x, y}; });
    });
}

void instance_rows(Ui& ui, const engine::script::Instance& instance, InstanceId id) {
    name_row(ui, instance, id);
    row(ui, CLAY_STRING("Type"), [&] {
        text(ui.theme(), view_of(instance.type), size::text, color::text_soft);
    });
    position_row(ui, instance.position, id);
    if (instance.overrides.empty()) {
        return;
    }
    row(ui, CLAY_STRING("Overrides"), [&] {
        if (button(ui, CLAY_ID("InspectorResetAll"), CLAY_STRING("Reset all"),
                   {.background = color::field, .foreground = color::text_soft})) {
            ui.emit(edit::ResetAll{.id = id});
        }
    });
}

std::string_view kept(Ui& ui, std::string text) {
    const Clay_String stored = ui.keep(std::move(text));
    return {stored.chars, static_cast<std::size_t>(stored.length)};
}

void component_header(const Theme& theme, std::string_view component) {
    CLAY({.layout = {.padding = {.left = theme.px16(size::gutter),
                                 .top = theme.px16(component_gap),
                                 .bottom = theme.px16(row_padding)}}}) {
        text(theme, view_of(component), size::small_text, color::text_soft);
    }
}

void field_rows(Ui& ui, const InspectorModel& model) {
    if (model.fields.empty() && model.script_fields.empty()) {
        panel_header(ui.theme(), CLAY_STRING("FIELDS"));
        empty_hint(ui.theme(), CLAY_STRING("This prefab has no editable field"));
        return;
    }
    std::uint32_t index = 0;
    if (!model.fields.empty()) {
        panel_header(ui.theme(), CLAY_STRING("COMPONENTS"));
    }
    std::string_view component;
    for (const InspectorField& field : model.fields) {
        if (component_of(field.key) != component) {
            component = component_of(field.key);
            component_header(ui.theme(), component);
        }
        field_row(ui, {.id = model.id,
                       .field = field,
                       .index = index++,
                       .label = kept(ui, field_label(field.key))});
    }
    if (!model.script_fields.empty()) {
        panel_header(ui.theme(), CLAY_STRING("SCRIPT"));
    }
    for (const InspectorField& field : model.script_fields) {
        field_row(ui, {.id = model.id,
                       .field = field,
                       .index = index++,
                       .label = kept(ui, field_label(field.key))});
    }
}

} // namespace

void inspector(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    const std::optional<InspectorModel>& model = editor.state().inspector;
    const auto* selected = model ? editor.session().document().find(model->id) : nullptr;
    CLAY({.id = CLAY_ID("Inspector"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::inspector)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.left = theme.px16(1.F)}}}) {
        panel_header(theme, selected ? ui.keep(std::format("INSPECTOR · {}",
                                                           instance_label(selected->instance)))
                                     : CLAY_STRING("INSPECTOR"));
        if (model && selected) {
            instance_rows(ui, selected->instance, model->id);
            field_rows(ui, *model);
        } else {
            nothing_selected(theme);
        }
    }
}

} // namespace editor
