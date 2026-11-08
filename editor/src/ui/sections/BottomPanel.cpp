#include "ui/sections/BottomPanel.hpp"

#include <clay.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "ui/Labels.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/ListRow.hpp"
#include "viewport/ViewportState.hpp"

namespace editor {

namespace {

constexpr float thumbnail_size = 24.F;

void tab(Ui& ui, Clay_ElementId id, Clay_String label, BottomTab tab, BottomTab current) {
    const Theme& theme = ui.theme();
    const bool active = tab == current;
    CLAY({.id = id,
          .layout = {.sizing = {.height = CLAY_SIZING_GROW()},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .border = {.color = color::focus,
                     .width = {.bottom = active ? theme.px16(2.F) : std::uint16_t{0}}}}) {
        text(theme, label, size::text, active || Ui::hovered(id) ? color::text : color::text_muted);
    }
    if (ui.clicked(id) && !active) {
        ui.emit(command::ShowTab{.tab = tab});
    }
}

void tabs(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    const BottomTab current = editor.state().bottom_tab;
    const auto problems =
        editor.session().problems().size() + editor.session().script_messages().size();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::tabs))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)}},
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        tab(ui, CLAY_ID("ConsoleTab"), CLAY_STRING("Console"), BottomTab::console, current);
        tab(ui, CLAY_ID("ProblemsTab"),
            problems == 0 ? CLAY_STRING("Problems") : ui.keep(std::format("Problems {}", problems)),
            BottomTab::problems, current);
        tab(ui, CLAY_ID("AssetsTab"),
            ui.keep(std::format("Assets {}", editor.state().assets.size())), BottomTab::assets,
            current);
    }
}

void line(const Theme& theme, Clay_String content, Clay_Color tint) {
    CLAY({.layout = {
              .sizing = {.width = CLAY_SIZING_GROW()},
              .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)}}}) {
        text(theme, content, size::small_text, tint);
    }
}

Clay_Color line_color(Severity severity) {
    switch (severity) {
    case Severity::error:
        return color::error;
    case Severity::warning:
        return color::accent;
    case Severity::info:
        break;
    }
    return color::text_soft;
}

void console_line(Ui& ui, Clay_ElementId id, const ConsoleLine& entry) {
    const Theme& theme = ui.theme();
    const bool opens = !entry.script.empty();
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter)},
                     .childGap = theme.px16(12.F)},
          .backgroundColor = opens && Ui::hovered(id) ? color::hover : Clay_Color{}}) {
        if (!entry.time.empty()) {
            text(theme, view_of(entry.time), size::small_text, color::text_dim, Face::regular);
        }
        text(theme, view_of(entry.text), size::small_text, line_color(entry.severity));
    }
    if (opens && ui.clicked(id)) {
        ui.emit(command::OpenScript{.file = entry.script});
    }
}

void console(Ui& ui, const std::vector<ConsoleLine>& lines) {
    if (lines.empty()) {
        empty_hint(ui.theme(), CLAY_STRING("Nothing to report"));
    }
    for (std::size_t index = 0; index < lines.size(); ++index) {
        console_line(ui, CLAY_IDI("ConsoleLine", static_cast<std::uint32_t>(index)), lines[index]);
    }
}

std::string describe(const engine::editor::Problem& problem,
                     const engine::editor::EditorSession& session) {
    const auto* entry = session.document().find(problem.instance);
    const std::string owner = entry ? std::string(instance_label(entry->instance)) : "?";
    if (problem.field.empty()) {
        return std::format("{}: {}", owner, problem.message);
    }
    return std::format("{} {}: {}", owner, problem.field, problem.message);
}

void problems(Ui& ui, const engine::editor::EditorSession& session) {
    if (session.problems().empty() && session.script_messages().empty()) {
        empty_hint(ui.theme(), CLAY_STRING("No problems"));
    }
    for (const auto& message : session.script_messages()) {
        line(ui.theme(), view_of(message.text), message.error ? color::error : color::accent);
    }
    for (const auto& problem : session.problems()) {
        line(ui.theme(), ui.keep(describe(problem, session)), color::error);
    }
}

void thumbnail(const Theme& theme, const AssetEntry& entry) {
    const float side = theme.px(thumbnail_size);
    CLAY(
        {.layout = {.sizing = {.width = CLAY_SIZING_FIXED(side), .height = CLAY_SIZING_FIXED(side)},
                    .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
         .backgroundColor = color::field,
         .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius))}) {
        if (entry.kind == AssetKind::sprite) {
            CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}},
                  .image = {.imageData = const_cast<AssetEntry*>(&entry)}}) {}
        } else {
            text(theme, view_of(kind_label(entry.kind).substr(0, 3)), size::small_text,
                 color::text_dim);
        }
    }
}

void react_to_asset(Ui& ui, const Editor& editor, Clay_ElementId id, const AssetEntry& entry) {
    if (!ui.clicked(id)) {
        return;
    }
    if (ui.input().click_count == 2) {
        ui.emit(command::OpenScript{.file = editor.session().game() / "assets" / entry.path});
        return;
    }
    if (const auto key = asset_field(entry.kind)) {
        ui.widgets().viewport().asset_drag =
            AssetDrag{.path = entry.path, .value = asset_value(entry), .key = std::string(*key)};
    }
}

void asset_row(Ui& ui, const Editor& editor, Clay_ElementId id, const AssetEntry& entry) {
    const Theme& theme = ui.theme();
    const auto& dragged = ui.widgets().viewport().asset_drag;
    const bool picked = dragged && dragged->path == entry.path;
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = {.left = theme.px16(size::gutter),
                                 .right = theme.px16(size::gutter),
                                 .top = theme.px16(2.F),
                                 .bottom = theme.px16(2.F)},
                     .childGap = theme.px16(10.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = picked            ? color::selected
                             : Ui::hovered(id) ? color::hover
                                               : Clay_Color{}}) {
        thumbnail(theme, entry);
        text(theme, view_of(entry.path), size::small_text, color::text);
        spacer();
        text(theme, view_of(kind_label(entry.kind)), size::small_text, color::text_dim);
    }
    react_to_asset(ui, editor, id, entry);
}

void assets(Ui& ui, const Editor& editor) {
    const auto& entries = editor.state().assets;
    if (entries.empty()) {
        empty_hint(ui.theme(), CLAY_STRING("No files in assets/"));
    }
    for (std::size_t index = 0; index < entries.size(); ++index) {
        asset_row(ui, editor, CLAY_IDI("AssetRow", static_cast<std::uint32_t>(index)),
                  entries[index]);
    }
}

} // namespace

float bottom_panel_height(const Theme& theme, float window_height) {
    const float others =
        theme.px(size::menu_bar + size::toolbar + size::status_bar + size::workspace_min);
    return std::clamp(window_height - others, theme.px(size::bottom_panel_min),
                      theme.px(size::bottom_panel));
}

void bottom_panel(Ui& ui, const Editor& editor, float height) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("BottomPanel"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIXED(height)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::toolbar,
          .border = {.color = color::border, .width = {.top = theme.px16(1.F)}}}) {
        tabs(ui, editor);
        CLAY({.id = CLAY_ID("BottomContent"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .padding = {.top = theme.px16(4.F), .bottom = theme.px16(4.F)},
                         .childGap = theme.px16(2.F),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = scroll_clip(ui.widgets().scroll(), CLAY_ID("BottomContent"))}) {
            switch (editor.state().bottom_tab) {
            case BottomTab::console:
                console(ui, editor.state().console);
                break;
            case BottomTab::problems:
                problems(ui, editor.session());
                break;
            case BottomTab::assets:
                assets(ui, editor);
                break;
            }
        }
    }
}

} // namespace editor
