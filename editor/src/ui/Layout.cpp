#include "ui/Layout.hpp"

#include "ui/sections/BottomPanel.hpp"
#include "ui/sections/Inspector.hpp"
#include "ui/sections/MenuBar.hpp"
#include "ui/sections/Sidebar.hpp"
#include "ui/sections/StatusBar.hpp"
#include "ui/sections/Toolbar.hpp"
#include "ui/sections/Viewport.hpp"

namespace editor {

Clay_RenderCommandArray build_layout(const Theme& theme, Clay_Dimensions window) {
    Clay_SetLayoutDimensions(window);
    Clay_BeginLayout();
    CLAY({.id = CLAY_ID("Root"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        menu_bar(theme);
        toolbar(theme);
        CLAY({.id = CLAY_ID("Workspace"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}}}) {
            sidebar(theme);
            viewport();
            inspector(theme);
        }
        bottom_panel(theme);
        status_bar(theme);
    }
    return Clay_EndLayout();
}

} // namespace editor
