#include "ui/sections/CommandPalette.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "text/FuzzyMatch.hpp"
#include "ui/CommandTable.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

constexpr std::int16_t palette_layer = 300;
constexpr std::int16_t field_layer = palette_layer + 1;
constexpr float palette_width = 560.F;
constexpr float palette_top = 80.F;
constexpr float row_gap = 4.F;
constexpr float panel_padding = 6.F;
constexpr Clay_Color backdrop{.r = 0, .g = 0, .b = 0, .a = 90};

Clay_ElementId query_id() { return CLAY_ID("PaletteQuery"); }

Clay_ElementId panel_id() { return CLAY_ID("Palette"); }

Clay_ElementId row_id(std::size_t entry) {
    return CLAY_IDI("PaletteRow", static_cast<std::uint32_t>(entry));
}

bool opens(Ui& ui, const Editor& editor) {
    const UiInput& input = ui.input();
    return !ui.widgets().wants_keyboard() && !editor.state().export_dialog &&
           input.primary_modifier == palette_shortcut.primary_modifier &&
           input.shift == palette_shortcut.shift &&
           std::ranges::find(input.keys_pressed, palette_shortcut.key) != input.keys_pressed.end();
}

void open(Ui& ui) {
    ui.widgets().palette() = PaletteState{.open = true, .query = {}, .highlighted = 0};
    ui.widgets().focus(query_id(), "");
}

void close(Ui& ui) {
    ui.widgets().palette() = PaletteState{};
    if (ui.widgets().is_focused(query_id())) {
        ui.widgets().blur();
    }
}

std::size_t rows_that_fit(const Theme& theme, Clay_Dimensions window) {
    const float row = theme.px(size::row) + theme.px(row_gap);
    const float taken =
        (2.F * theme.px(palette_top)) + (2.F * theme.px(panel_padding)) + theme.px(size::row);
    const float room = window.height - taken;
    return room < row ? 1 : static_cast<std::size_t>(room / row);
}

std::vector<std::size_t> matches(std::string_view query, std::size_t max_rows) {
    const auto table = command_table();
    std::vector<std::string_view> labels;
    labels.reserve(table.size());
    for (const CommandEntry& entry : table) {
        labels.push_back(entry.label);
    }
    std::vector<std::size_t> ranked = fuzzy_rank(query, labels);
    if (ranked.size() > max_rows) {
        ranked.resize(max_rows);
    }
    return ranked;
}

void move_highlight(const UiInput& input, std::size_t count, std::size_t& highlighted) {
    if (pressed_or_repeated(input, Key::up) && highlighted > 0) {
        --highlighted;
    }
    if (pressed_or_repeated(input, Key::down) && highlighted + 1 < count) {
        ++highlighted;
    }
}

Clay_Color row_background(bool highlighted, bool hovered) {
    if (highlighted) {
        return color::selected;
    }
    return hovered ? color::hover : Clay_Color{};
}

bool palette_row(Ui& ui, std::size_t entry_index, bool highlighted, bool enabled) {
    const Theme& theme = ui.theme();
    const CommandEntry& entry = command_table()[entry_index];
    const Clay_ElementId id = row_id(entry_index);
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)},
                     .childGap = theme.px16(10.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = row_background(highlighted, enabled && Ui::hovered(id)),
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(4.F))}) {
        text(theme, view_of(entry.label), size::text, enabled ? color::text : color::text_dim);
        text(theme, view_of(menu_title(entry.menu)), size::small_text, color::text_dim);
        spacer();
        if (entry.shortcut) {
            text(theme, ui.keep(shortcut_label(*entry.shortcut)), size::small_text,
                 color::text_muted);
        }
    }
    return enabled && ui.clicked(id);
}

struct Drawn {
    TextResult query;
    std::optional<std::size_t> clicked;
};

void rows(Ui& ui, const Editor& editor, const std::vector<std::size_t>& ranked,
          std::size_t highlighted, Drawn& drawn) {
    if (ranked.empty()) {
        CLAY({.layout = {.padding = CLAY_PADDING_ALL(ui.theme().px16(8.F))}}) {
            text(ui.theme(), CLAY_STRING("No command matches"), size::small_text,
                 color::text_muted);
        }
    }
    for (std::size_t row = 0; row < ranked.size(); ++row) {
        const std::size_t entry = ranked[row];
        const bool enabled = command_table()[entry].make(editor).has_value();
        if (palette_row(ui, entry, row == highlighted, enabled)) {
            drawn.clicked = entry;
        }
    }
}

Drawn draw(Ui& ui, const Editor& editor, Clay_Dimensions window,
           const std::vector<std::size_t>& ranked) {
    const Theme& theme = ui.theme();
    const PaletteState& palette = ui.widgets().palette();
    Drawn drawn;
    CLAY({.id = CLAY_ID("PaletteBackdrop"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(window.width),
                                .height = CLAY_SIZING_FIXED(window.height)},
                     .padding = {.top = theme.px16(palette_top)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER}},
          .backgroundColor = backdrop,
          .floating = {.zIndex = palette_layer, .attachTo = CLAY_ATTACH_TO_ROOT}}) {
        CLAY({.id = panel_id(),
              .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(palette_width))},
                         .padding = CLAY_PADDING_ALL(theme.px16(panel_padding)),
                         .childGap = theme.px16(row_gap),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = color::panel,
              .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
              .border = {.color = color::border,
                         .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
            drawn.query = text_field(ui, query_id(), palette.query,
                                     {.placeholder = "Type a command", .layer = field_layer});
            rows(ui, editor, ranked, palette.highlighted, drawn);
        }
    }
    return drawn;
}

void run(Ui& ui, const Editor& editor, std::size_t entry) {
    if (std::optional<Action> action = command_table()[entry].make(editor)) {
        ui.emit(std::move(*action));
        close(ui);
    } else {
        ui.widgets().focus(query_id(), ui.widgets().palette().query);
    }
}

void keep_focus(Ui& ui) {
    WidgetState& widgets = ui.widgets();
    widgets.cancel_tab();
    if (!widgets.is_focused(query_id())) {
        widgets.focus(query_id(), widgets.palette().query);
    }
}

void react(Ui& ui, const Editor& editor, const std::vector<std::size_t>& ranked,
           const Drawn& drawn, bool clicked_outside) {
    PaletteState& palette = ui.widgets().palette();
    const FieldEvent event = drawn.query.event;
    if (drawn.clicked) {
        run(ui, editor, *drawn.clicked);
    } else if (clicked_outside || event == FieldEvent::cancelled) {
        close(ui);
    } else if (event == FieldEvent::committed && pressed_or_repeated(ui.input(), Key::enter) &&
               !ranked.empty()) {
        run(ui, editor, ranked[palette.highlighted]);
    } else if (event == FieldEvent::committed) {
        keep_focus(ui);
    } else if (event == FieldEvent::changed) {
        palette.query = drawn.query.value;
        palette.highlighted = 0;
    }
}

} // namespace

void command_palette(Ui& ui, const Editor& editor, Clay_Dimensions window) {
    PaletteState& palette = ui.widgets().palette();
    if (!palette.open) {
        if (!opens(ui, editor)) {
            return;
        }
        open(ui);
    }
    const std::vector<std::size_t> ranked =
        matches(palette.query, rows_that_fit(ui.theme(), window));
    palette.highlighted = std::min(palette.highlighted, ranked.empty() ? 0 : ranked.size() - 1);
    move_highlight(ui.input(), ranked.size(), palette.highlighted);
    const bool clicked_outside = ui.input().primary_pressed && !Ui::hovered(panel_id());
    const Drawn drawn = draw(ui, editor, window, ranked);
    react(ui, editor, ranked, drawn, clicked_outside);
}

} // namespace editor
