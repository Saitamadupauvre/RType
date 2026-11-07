#include "ui/sections/Toolbar.hpp"

#include <clay.h>

#include <format>

#include "ui/Text.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/Toggle.hpp"

namespace editor {

namespace {

constexpr ButtonStyle plain{.background = color::toolbar, .foreground = color::text_soft};

void tool_button(Ui& ui, Clay_ElementId id, Clay_String key, Tool tool, Tool current) {
    const Theme& theme = ui.theme();
    const bool active = tool == current;
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::tool_button)),
                                .height = CLAY_SIZING_FIXED(theme.px(size::tool_button))},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = active            ? color::selected
                             : Ui::hovered(id) ? color::hover
                                               : Clay_Color{},
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius))}) {
        text(theme, key, size::text, active ? color::focus : color::text_soft);
    }
    if (ui.clicked(id)) {
        ui.emit(command::SetTool{.tool = tool});
    }
}

void separator(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(1.F)),
                                .height = CLAY_SIZING_FIXED(theme.px(size::separator))}},
          .backgroundColor = color::border}) {}
}

void tools(Ui& ui, Tool current) {
    tool_button(ui, CLAY_ID("ToolSelect"), CLAY_STRING("V"), Tool::select, current);
    tool_button(ui, CLAY_ID("ToolMove"), CLAY_STRING("G"), Tool::move, current);
    tool_button(ui, CLAY_ID("ToolPan"), CLAY_STRING("H"), Tool::pan, current);
}

void grid(Ui& ui, const ViewSettings& view) {
    if (toggle(ui, CLAY_ID("SnapToggle"), CLAY_STRING("Snap"), view.snap)) {
        ui.emit(command::SetSnap{.enabled = !view.snap});
    }
    text(ui.theme(), ui.keep(std::format("Grid {:g} px", view.grid_size)), size::text,
         color::text_muted);
}

void overlays(Ui& ui, const ViewSettings& view) {
    if (toggle(ui, CLAY_ID("HitboxesToggle"), CLAY_STRING("Hitboxes"), view.show_hitboxes)) {
        ui.emit(command::SetOverlay{.overlay = Overlay::hitboxes, .visible = !view.show_hitboxes});
    }
    if (toggle(ui, CLAY_ID("NamesToggle"), CLAY_STRING("Names"), view.show_names)) {
        ui.emit(command::SetOverlay{.overlay = Overlay::names, .visible = !view.show_names});
    }
}

void history(Ui& ui, const Editor& editor) {
    const bool editing = editor.state().mode == Mode::editing;
    const auto& session = editor.session();
    if (button(ui, CLAY_ID("UndoButton"), CLAY_STRING("Undo"), plain,
               editing && session.can_undo())) {
        ui.emit(command::Undo{});
    }
    if (button(ui, CLAY_ID("RedoButton"), CLAY_STRING("Redo"), plain,
               editing && session.can_redo())) {
        ui.emit(command::Redo{});
    }
}

} // namespace

void toolbar(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("Toolbar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::toolbar))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)},
                     .childGap = theme.px16(8.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::toolbar,
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        tools(ui, editor.state().tool);
        separator(theme);
        grid(ui, editor.state().view);
        separator(theme);
        overlays(ui, editor.state().view);
        spacer();
        history(ui, editor);
    }
}

} // namespace editor
