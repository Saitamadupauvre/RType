#include "ui/sections/Inspector.hpp"

#include <clay.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "text/FitText.hpp"
#include "ui/FieldText.hpp"
#include "ui/Labels.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/Text.hpp"
#include "ui/sections/InspectorFields.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/Dropdown.hpp"
#include "ui/widgets/ListRow.hpp"
#include "ui/widgets/NumberField.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

constexpr float label_width = 120.F;

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
                         .childGap = theme.px16(4.F),
                         .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
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

void type_row(Ui& ui, const Editor& editor, const engine::script::Instance& instance,
              InstanceId id) {
    const std::vector<std::string>& prefabs = editor.state().prefabs;
    const auto current = std::ranges::find(prefabs, instance.type);
    row(ui, CLAY_STRING("Type"), [&] {
        if (current == prefabs.end()) {
            text(ui.theme(), view_of(instance.type), size::text, color::text_soft);
            return;
        }
        const std::vector<std::string_view> options(prefabs.begin(), prefabs.end());
        const auto picked = dropdown(ui, CLAY_ID("InspectorType"),
                                     static_cast<std::size_t>(current - prefabs.begin()), options);
        if (picked && prefabs[*picked] != instance.type) {
            ui.emit(edit::ChangeType{.id = id, .type = prefabs[*picked]});
        }
    });
}

std::filesystem::path shown_script(const Editor& editor, const std::filesystem::path& script) {
    const std::filesystem::path relative =
        script.lexically_relative(editor.session().game() / "scripts");
    return relative.empty() ? script.filename() : relative;
}

Clay_String fitted_name(Ui& ui, std::string_view name) {
    const Clay_ElementData panel = Clay_GetElementData(CLAY_ID("Inspector"));
    if (!panel.found) {
        return view_of(name);
    }
    const Theme& theme = ui.theme();
    Clay_TextElementConfig font{};
    font.fontId = static_cast<std::uint16_t>(Face::bold);
    font.fontSize = theme.px16(size::text);
    const float width = panel.boundingBox.width - (2.F * theme.px(size::gutter));
    return ui.keep(fit_text(name, width, [&ui, &font](std::string_view prefix) {
        return ui.platform().measure(prefix, font).width;
    }));
}

void identity(Ui& ui, const Editor& editor, const engine::script::Instance& instance,
              const InspectorModel& model) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter),
                                 .top = theme.px16(size::gutter),
                                 .bottom = theme.px16(size::gutter)},
                     .childGap = theme.px16(4.F),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        text(theme, fitted_name(ui, instance_label(instance)), size::text, color::text, Face::bold);
        const std::string subtitle =
            model.script.empty() ? std::format("instance of {}", instance.type)
                                 : std::format("instance of {} · {}", instance.type,
                                               shown_script(editor, model.script).generic_string());
        wrapped_text(theme, ui.keep(subtitle), size::small_text, color::text_muted);
        wrapped_text(theme,
                     CLAY_STRING("orange = overridden, saved in the scene · grey = prefab value"),
                     size::small_text, color::text_dim);
    }
}

bool wide_button(Ui& ui, Clay_ElementId id, Clay_String label) {
    const Theme& theme = ui.theme();
    bool clicked = false;
    CLAY({.layout = {
              .sizing = {.width = CLAY_SIZING_GROW()},
              .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)}}}) {
        clicked =
            button(ui, id, label, {.background = color::field, .foreground = color::text_soft});
    }
    return clicked;
}

void footer(Ui& ui, const engine::script::Instance& instance, const InspectorModel& model) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = {.top = theme.px16(size::gutter)},
                     .childGap = theme.px16(6.F),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        if (!instance.overrides.empty() &&
            wide_button(ui, CLAY_ID("InspectorResetAll"), CLAY_STRING("Reset all to prefab"))) {
            ui.emit(edit::ResetAll{.id = model.id});
        }
        if (!model.script.empty() &&
            wide_button(ui, CLAY_ID("InspectorOpenScript"),
                        ui.keep(std::format("Open {}", model.script.filename().string())))) {
            ui.emit(command::OpenScript{.file = model.script});
        }
    }
}

void instance_rows(Ui& ui, const Editor& editor, const engine::script::Instance& instance,
                   InstanceId id) {
    name_row(ui, instance, id);
    type_row(ui, editor, instance, id);
    position_row(ui, instance.position, id);
}

std::string_view kept(Ui& ui, std::string text) {
    const Clay_String stored = ui.keep(std::move(text));
    return {stored.chars, static_cast<std::size_t>(stored.length)};
}

std::size_t overridden_in(const std::vector<InspectorField>& fields, std::string_view component) {
    return static_cast<std::size_t>(std::ranges::count_if(fields, [&](const InspectorField& field) {
        return field.overridden && component_of(field.key) == component;
    }));
}

void band(Ui& ui, Clay_String name, std::size_t overridden) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::band))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::band,
          .border = {.color = color::border, .width = {.top = theme.px16(1.F)}}}) {
        text(theme, name, size::small_text, color::text, Face::bold);
        spacer();
        if (overridden > 0) {
            text(theme, ui.keep(std::format("{} overridden", overridden)), size::small_text,
                 color::accent);
        }
    }
}

void field_rows(Ui& ui, const InspectorModel& model) {
    if (model.fields.empty() && model.script_fields.empty()) {
        panel_header(ui.theme(), CLAY_STRING("FIELDS"));
        empty_hint(ui.theme(), CLAY_STRING("This prefab has no editable field"));
        return;
    }
    std::uint32_t index = 0;
    std::string_view component;
    for (const InspectorField& field : model.fields) {
        if (component_of(field.key) != component) {
            component = component_of(field.key);
            band(ui, view_of(component), overridden_in(model.fields, component));
        }
        field_row(ui, {.id = model.id,
                       .field = field,
                       .index = index++,
                       .label = kept(ui, field_label(field.key))});
    }
    if (!model.script_fields.empty()) {
        band(ui, CLAY_STRING("Script fields"), overridden_in(model.script_fields, "Script"));
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
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(theme.px(size::inspector_min),
                                                          theme.px(size::inspector)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.left = theme.px16(1.F)}}}) {
        panel_header(theme, CLAY_STRING("INSPECTOR"));
        CLAY({.id = CLAY_ID("InspectorBody"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .padding = {.bottom = theme.px16(size::gutter)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = scroll_clip(ui.widgets().scroll(), CLAY_ID("InspectorBody"), false)}) {
            if (model && selected) {
                identity(ui, editor, selected->instance, *model);
                instance_rows(ui, editor, selected->instance, model->id);
                field_rows(ui, *model);
                footer(ui, selected->instance, *model);
            } else {
                nothing_selected(theme);
            }
        }
    }
}

} // namespace editor
