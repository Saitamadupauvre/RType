#include "ui/sections/Inspector.hpp"

#include <clay.h>

#include <format>

#include "ui/Labels.hpp"
#include "ui/Text.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/ListRow.hpp"

namespace editor {

namespace {

void nothing_selected(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}) {
        text(theme, CLAY_STRING("Nothing selected"), size::text, color::text_muted);
    }
}

void property(const Theme& theme, Clay_String name, Clay_String value) {
    list_item(theme, {.label = name, .detail = value});
}

void details(Ui& ui, const engine::script::Instance& instance) {
    const Theme& theme = ui.theme();
    property(theme, CLAY_STRING("Name"),
             instance.name.empty() ? CLAY_STRING("none") : view_of(instance.name));
    property(theme, CLAY_STRING("Type"), view_of(instance.type));
    property(theme, CLAY_STRING("Position"),
             ui.keep(std::format("{:g}, {:g}", instance.position.x, instance.position.y)));
    panel_header(theme, CLAY_STRING("OVERRIDES"));
    if (instance.overrides.empty()) {
        empty_hint(theme, CLAY_STRING("Every field uses the prefab value"));
    }
    for (const auto& override : instance.overrides) {
        list_item(theme, {.label = view_of(override.field)});
    }
}

} // namespace

void inspector(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    const auto& session = editor.session();
    const auto selection = session.selection();
    const auto* selected = selection ? session.document().find(*selection) : nullptr;
    CLAY({.id = CLAY_ID("Inspector"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::inspector)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.left = theme.px16(1.F)}}}) {
        panel_header(theme, selected ? ui.keep(std::format("INSPECTOR · {}",
                                                           instance_label(selected->instance)))
                                     : CLAY_STRING("INSPECTOR"));
        if (selected) {
            details(ui, selected->instance);
        } else {
            nothing_selected(theme);
        }
    }
}

} // namespace editor
