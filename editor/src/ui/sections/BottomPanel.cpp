#include "ui/sections/BottomPanel.hpp"

#include <clay.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

#include "ui/Labels.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/ListRow.hpp"

namespace editor {

namespace {

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
    const auto problems = editor.session().problems().size();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::tabs))},
                     .padding = {.left = theme.px16(8.F), .right = theme.px16(8.F)}},
          .border = {.color = color::border, .width = {.bottom = theme.px16(1.F)}}}) {
        tab(ui, CLAY_ID("ConsoleTab"), CLAY_STRING("Console"), BottomTab::console, current);
        tab(ui, CLAY_ID("ProblemsTab"),
            problems == 0 ? CLAY_STRING("Problems") : ui.keep(std::format("Problems {}", problems)),
            BottomTab::problems, current);
    }
}

void line(const Theme& theme, Clay_String content, Clay_Color tint) {
    CLAY({.layout = {
              .sizing = {.width = CLAY_SIZING_GROW()},
              .padding = {.left = theme.px16(size::gutter), .right = theme.px16(size::gutter)}}}) {
        text(theme, content, size::small_text, tint);
    }
}

void console(const Theme& theme, const std::vector<ConsoleLine>& lines) {
    if (lines.empty()) {
        empty_hint(theme, CLAY_STRING("Nothing to report"));
    }
    for (const ConsoleLine& entry : lines) {
        line(theme, view_of(entry.text),
             entry.severity == Severity::error ? color::error : color::text_soft);
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
    if (session.problems().empty()) {
        empty_hint(ui.theme(), CLAY_STRING("No problems"));
    }
    for (const auto& problem : session.problems()) {
        line(ui.theme(), ui.keep(describe(problem, session)), color::error);
    }
}

} // namespace

void bottom_panel(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("BottomPanel"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::bottom_panel))},
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
            if (editor.state().bottom_tab == BottomTab::console) {
                console(theme, editor.state().console);
            } else {
                problems(ui, editor.session());
            }
        }
    }
}

} // namespace editor
