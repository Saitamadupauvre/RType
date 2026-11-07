#include "ui/sections/MenuBar.hpp"

#include <clay.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ui/CommandTable.hpp"
#include "ui/Labels.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/Button.hpp"

namespace editor {

namespace {

constexpr ButtonStyle primary{.background = color::accent, .foreground = color::text_on_accent};
constexpr ButtonStyle secondary{.background = color::toolbar, .foreground = color::text};

constexpr std::int16_t popup_layer = 100;
constexpr float menu_width = 240.F;
constexpr float check_width = 18.F;
constexpr std::string_view check_mark = "\xE2\x9C\x93";

constexpr std::array<std::string_view, 5> menu_titles{"File", "Edit", "View", "Scene", "Help"};
constexpr std::array menus{Menu::file, Menu::edit, Menu::view, Menu::scene};

struct HelpLine {
    std::string_view action;
    std::string_view keys;
};

constexpr std::array help_lines{
    HelpLine{.action = "Pan the view", .keys = "Middle drag"},
    HelpLine{.action = "Zoom", .keys = "Wheel"},
    HelpLine{.action = "Place a prefab", .keys = "Click it, then click"},
    HelpLine{.action = "Move without snap", .keys = "Alt+drag"},
    HelpLine{.action = "Cancel placing", .keys = "Esc"},
};

Clay_ElementId title_id(std::size_t menu) {
    return CLAY_IDI("MenuTitle", static_cast<std::uint32_t>(menu));
}

Clay_ElementId list_id() { return CLAY_ID("MenuList"); }

Clay_ElementId entry_id(std::size_t index) {
    return CLAY_IDI("MenuEntry", static_cast<std::uint32_t>(index));
}

std::optional<std::size_t> open_menu(Ui& ui) {
    for (std::size_t menu = 0; menu < menu_titles.size(); ++menu) {
        if (ui.widgets().is_open(title_id(menu))) {
            return menu;
        }
    }
    return std::nullopt;
}

void update_menus(Ui& ui) {
    WidgetState& widgets = ui.widgets();
    const std::optional<std::size_t> open = open_menu(ui);
    for (std::size_t menu = 0; menu < menu_titles.size(); ++menu) {
        if (ui.clicked(title_id(menu))) {
            if (open == menu) {
                widgets.close();
            } else {
                widgets.open(title_id(menu), 0);
            }
            return;
        }
        if (open && open != menu && Ui::hovered(title_id(menu))) {
            widgets.open(title_id(menu), 0);
            return;
        }
    }
    const UiInput& input = ui.input();
    if (open && (pressed_or_repeated(input, Key::escape) ||
                 (input.primary_pressed && !Ui::hovered(list_id())))) {
        widgets.close();
    }
}

void menu_row(const Theme& theme, Clay_ElementId id, bool active, auto content) {
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(6.F), .right = theme.px16(10.F)},
                     .childGap = theme.px16(12.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = active && Ui::hovered(id) ? color::hover : Clay_Color{},
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(4.F))}) {
        content();
    }
}

void check_column(const Theme& theme, bool checked) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(check_width))}}}) {
        if (checked) {
            text(theme, view_of(check_mark), size::text, color::focus);
        }
    }
}

void entry_row(Ui& ui, const Editor& editor, std::size_t index, const CommandEntry& entry) {
    const Theme& theme = ui.theme();
    std::optional<Action> action = entry.make(editor);
    const bool enabled = action.has_value();
    if (enabled && ui.clicked(entry_id(index))) {
        ui.emit(std::move(*action));
        ui.widgets().close();
    }
    menu_row(theme, entry_id(index), enabled, [&] {
        check_column(theme, entry.checked != nullptr && entry.checked(editor));
        text(theme, view_of(entry.label), size::text, enabled ? color::text : color::text_dim);
        spacer();
        if (entry.shortcut) {
            text(theme, ui.keep(shortcut_label(*entry.shortcut)), size::small_text,
                 color::text_muted);
        }
    });
}

void help_rows(Ui& ui) {
    const Theme& theme = ui.theme();
    for (std::size_t index = 0; index < help_lines.size(); ++index) {
        const HelpLine& line = help_lines[index];
        menu_row(theme, CLAY_IDI("MenuHelp", static_cast<std::uint32_t>(index)), false, [&] {
            text(theme, view_of(line.action), size::text, color::text);
            spacer();
            text(theme, view_of(line.keys), size::small_text, color::text_muted);
        });
    }
}

void command_rows(Ui& ui, const Editor& editor, Menu menu) {
    const auto table = command_table();
    for (std::size_t index = 0; index < table.size(); ++index) {
        if (table[index].menu == menu) {
            entry_row(ui, editor, index, table[index]);
        }
    }
}

void menu_list(Ui& ui, const Editor& editor, std::size_t menu) {
    const Theme& theme = ui.theme();
    CLAY({.id = list_id(),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(menu_width))},
                     .padding = CLAY_PADDING_ALL(theme.px16(4.F)),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .floating = {.offset = {.y = theme.px(4.F)},
                       .zIndex = popup_layer,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                        .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM},
                       .attachTo = CLAY_ATTACH_TO_PARENT,
                       .clipTo = CLAY_CLIP_TO_NONE},
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        if (menu < menus.size()) {
            command_rows(ui, editor, menus[menu]);
        } else {
            help_rows(ui);
        }
    }
}

void menu_title(Ui& ui, const Editor& editor, std::size_t menu, bool open) {
    const Theme& theme = ui.theme();
    const Clay_ElementId id = title_id(menu);
    Clay_Color background{};
    if (open) {
        background = color::selected;
    } else if (Ui::hovered(id)) {
        background = color::hover;
    }
    CLAY({.id = id,
          .layout = {.sizing = {.height = CLAY_SIZING_FIXED(theme.px(size::button))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = background,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(4.F))}) {
        text(theme, view_of(menu_titles[menu]), size::text, open ? color::text : color::text_soft);
        if (open) {
            menu_list(ui, editor, menu);
        }
    }
}

void menus_row(Ui& ui, const Editor& editor) {
    update_menus(ui);
    const std::optional<std::size_t> open = open_menu(ui);
    CLAY({.layout = {.childGap = ui.theme().px16(2.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        for (std::size_t menu = 0; menu < menu_titles.size(); ++menu) {
            menu_title(ui, editor, menu, open == menu);
        }
    }
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
        menus_row(ui, editor);
        spacer();
        text(theme, ui.keep(scene_title(editor.session())), size::small_text, color::text_muted);
        spacer();
        run_buttons(ui, editor.state().mode);
        disabled_button(theme, CLAY_STRING("Export..."));
    }
}

} // namespace editor
