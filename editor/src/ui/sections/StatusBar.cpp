#include "ui/sections/StatusBar.hpp"

#include <clay.h>

#include <format>

#include "ui/Labels.hpp"
#include "ui/Text.hpp"

namespace editor {

void status_bar(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    const auto& session = editor.session();
    CLAY({.id = CLAY_ID("StatusBar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::status_bar))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(18.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::menu_bar,
          .border = {.color = color::border, .width = {.top = theme.px16(1.F)}}}) {
        text(theme, view_of(mode_label(editor.state().mode)), size::small_text, color::text_muted);
        if (session.open_scene_name()) {
            text(theme, ui.keep(std::format("{} instances", session.document().instances.size())),
                 size::small_text, color::text_muted);
        }
        spacer();
        text(theme, ui.keep(session.game().filename().string()), size::small_text,
             color::text_muted);
        text(theme, ui.keep(scene_title(session)), size::small_text, color::text_muted);
    }
}

} // namespace editor
