#include "ui/Layout.hpp"

#include "ui/CommandTable.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/Text.hpp"
#include "ui/sections/BottomPanel.hpp"
#include "ui/sections/CommandPalette.hpp"
#include "ui/sections/ExportDialog.hpp"
#include "ui/sections/Inspector.hpp"
#include "ui/sections/MenuBar.hpp"
#include "ui/sections/Sidebar.hpp"
#include "ui/sections/StatusBar.hpp"
#include "ui/sections/Toolbar.hpp"
#include "ui/sections/Viewport.hpp"

namespace editor {

namespace {

void feed_input(Ui& ui) {
    const UiInput& input = ui.input();
    Clay_SetPointerState(input.pointer, input.primary_down);
    Clay_UpdateScrollContainers(false, {}, input.delta_time);
    SmoothScroll& scroll = ui.widgets().scroll();
    if (Clay_IsDebugModeEnabled()) {
        scroll.add(CLAY_ID("Clay__DebugViewOuterScrollPane"));
    }
    scroll.update(input.wheel.y, ui.theme().px(SmoothScroll::step), input.delta_time);
}

void play_banner(Ui& ui, const Editor& editor) {
    const Mode mode = editor.state().mode;
    if (mode == Mode::editing) {
        return;
    }
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("PlayBanner"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::band))},
                     .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(12.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::selected,
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        text(theme, mode == Mode::playing ? CLAY_STRING("PLAYING") : CLAY_STRING("PAUSED"),
             size::small_text, color::focus, Face::bold);
        text(theme,
             CLAY_STRING("edits are blocked · the game runs in its own window · Stop ends it and "
                         "leaves the scene as it was"),
             size::small_text, color::text_soft);
    }
}

} // namespace

Clay_RenderCommandArray build_frame(Ui& ui, const Editor& editor, Clay_Dimensions window) {
    Clay_SetLayoutDimensions(window);
    feed_input(ui);
    emit_shortcuts(ui, editor);
    Clay_BeginLayout();
    CLAY({.id = CLAY_ID("Root"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        menu_bar(ui, editor);
        toolbar(ui, editor);
        play_banner(ui, editor);
        CLAY({.id = CLAY_ID("Workspace"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {
            sidebar(ui, editor);
            viewport(ui, editor);
            inspector(ui, editor);
        }
        bottom_panel(ui, editor, bottom_panel_height(ui.theme(), window.height));
        status_bar(ui, editor);
        export_dialog(ui, editor, window);
        command_palette(ui, editor, window);
    }
    const Clay_RenderCommandArray commands = Clay_EndLayout();
    ui.widgets().end_frame();
    return commands;
}

} // namespace editor
