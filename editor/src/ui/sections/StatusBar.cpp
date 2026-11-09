#include "ui/sections/StatusBar.hpp"

#include <clay.h>

#include <format>

#include "ui/Labels.hpp"
#include "ui/Text.hpp"

namespace editor {

namespace {

void selection(Ui& ui, const engine::editor::EditorSession& session) {
    const auto selected = session.selection();
    const auto* entry = selected ? session.document().find(*selected) : nullptr;
    if (entry == nullptr) {
        return;
    }
    text(ui.theme(),
         ui.keep(std::format("selected: {} ({})", instance_label(entry->instance),
                             entry->instance.type)),
         size::small_text, color::text_soft);
}

} // namespace

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
        const EditorState& state = editor.state();
        const bool editing = state.mode == Mode::editing;
        text(theme, view_of(mode_label(state.mode)), size::small_text,
             editing ? color::text_muted : color::focus);
        if (!editing) {
            text(theme, CLAY_STRING("the game runs in its own window"), size::small_text,
                 color::text_muted);
            text(theme, CLAY_STRING("scene untouched"), size::small_text, color::text_muted);
        } else if (session.open_scene_name()) {
            text(theme, ui.keep(std::format("{} instances", session.document().instances.size())),
                 size::small_text, color::text_muted);
            selection(ui, session);
            text(theme,
                 state.view.snap ? ui.keep(std::format("snap {:g}", state.view.grid_size))
                                 : CLAY_STRING("snap off"),
                 size::small_text, color::text_muted);
        }
        spacer();
        if (session.unsaved()) {
            text(theme, CLAY_STRING("unsaved changes · Ctrl+S"), size::small_text, color::accent);
        }
        text(theme, ui.keep(session.game().filename().string()), size::small_text,
             color::text_muted);
        text(theme, ui.keep(scene_title(session)), size::small_text, color::text_muted);
    }
}

} // namespace editor
