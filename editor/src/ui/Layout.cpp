#include "ui/Layout.hpp"

#include "ui/CommandTable.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/sections/BottomPanel.hpp"
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
        CLAY({.id = CLAY_ID("Workspace"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {
            sidebar(ui, editor);
            viewport(ui, editor);
            inspector(ui, editor);
        }
        bottom_panel(ui, editor, bottom_panel_height(ui.theme(), window.height));
        status_bar(ui, editor);
    }
    const Clay_RenderCommandArray commands = Clay_EndLayout();
    ui.widgets().end_frame();
    return commands;
}

} // namespace editor
