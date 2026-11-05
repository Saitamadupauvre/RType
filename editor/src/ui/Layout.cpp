#include "ui/Layout.hpp"

#include "ui/CommandTable.hpp"
#include "ui/sections/BottomPanel.hpp"
#include "ui/sections/Inspector.hpp"
#include "ui/sections/MenuBar.hpp"
#include "ui/sections/Sidebar.hpp"
#include "ui/sections/StatusBar.hpp"
#include "ui/sections/Toolbar.hpp"
#include "ui/sections/Viewport.hpp"

namespace editor {

namespace {

void feed_input(const UiInput& input) {
    Clay_SetPointerState(input.pointer, input.primary_down);
    Clay_UpdateScrollContainers(false, input.wheel, input.delta_time);
}

} // namespace

Clay_RenderCommandArray build_frame(Ui& ui, const Editor& editor, Clay_Dimensions window) {
    Clay_SetLayoutDimensions(window);
    feed_input(ui.input());
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
            viewport();
            inspector(ui, editor);
        }
        bottom_panel(ui, editor);
        status_bar(ui, editor);
    }
    return Clay_EndLayout();
}

} // namespace editor
