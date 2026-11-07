#include "ui/sections/MenuBar.hpp"

#include <clay.h>

#include "ui/Labels.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/Button.hpp"

namespace editor {

namespace {

constexpr ButtonStyle primary{.background = color::accent, .foreground = color::text_on_accent};
constexpr ButtonStyle secondary{.background = color::toolbar, .foreground = color::text};

void menu_item(const Theme& theme, Clay_String label) {
    text(theme, label, size::text, color::text_soft);
}

void run_buttons(Ui& ui, Mode mode) {
    if (button(ui, CLAY_ID("PlayButton"), CLAY_STRING("Play"), primary, mode != Mode::playing)) {
        ui.emit(command::Play{});
    }
    if (button(ui, CLAY_ID("StopButton"), CLAY_STRING("Stop"), secondary, mode != Mode::editing)) {
        ui.emit(command::Stop{});
    }
}

} // namespace

void menu_bar(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("MenuBar"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::menu_bar))},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(14.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::menu_bar,
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        text(theme, CLAY_STRING("engine editor"), size::text, color::text, Face::bold);
        menu_item(theme, CLAY_STRING("File"));
        menu_item(theme, CLAY_STRING("Edit"));
        menu_item(theme, CLAY_STRING("View"));
        menu_item(theme, CLAY_STRING("Scene"));
        menu_item(theme, CLAY_STRING("Help"));
        spacer();
        text(theme, ui.keep(scene_title(editor.session())), size::small_text, color::text_muted);
        spacer();
        run_buttons(ui, editor.state().mode);
        disabled_button(theme, CLAY_STRING("Export..."));
    }
}

} // namespace editor
