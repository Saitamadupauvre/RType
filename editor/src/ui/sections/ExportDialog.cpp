#include "ui/sections/ExportDialog.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "ui/Text.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/TextField.hpp"
#include "ui/widgets/Toggle.hpp"

namespace editor {

namespace {

constexpr std::int16_t dialog_layer = 200;
constexpr std::int16_t field_layer = dialog_layer + 1;
constexpr float dialog_width = 520.F;
constexpr float label_width = 110.F;
constexpr Clay_Color backdrop{.r = 0, .g = 0, .b = 0, .a = 150};
constexpr ButtonStyle primary{.background = color::accent, .foreground = color::text_on_accent};
constexpr ButtonStyle secondary{.background = color::toolbar, .foreground = color::text};

void change(Ui& ui, ExportSettings settings) {
    ui.emit(command::SetExport{.settings = std::move(settings)});
}

template <typename Field> void labelled(const Theme& theme, Clay_String label, Field field) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .childGap = theme.px16(8.F),
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(label_width))}}}) {
            text(theme, label, size::small_text, color::text_muted);
        }
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}}}) { field(); }
    }
}

void name_row(Ui& ui, const ExportSettings& settings) {
    labelled(ui.theme(), CLAY_STRING("Name"), [&] {
        const TextResult result =
            text_field(ui, CLAY_ID("ExportName"), settings.name, {.layer = field_layer});
        if (result.event != FieldEvent::none && result.value != settings.name) {
            ExportSettings changed = settings;
            changed.name = result.value;
            change(ui, std::move(changed));
        }
    });
}

void output_row(Ui& ui, const ExportSettings& settings) {
    labelled(ui.theme(), CLAY_STRING("Output folder"), [&] {
        const Clay_String kept = ui.keep(settings.output.string());
        const std::string_view output{kept.chars, static_cast<std::size_t>(kept.length)};
        const TextResult result =
            text_field(ui, CLAY_ID("ExportOutput"), output,
                       {.placeholder = "folder to create", .layer = field_layer});
        if (result.event != FieldEvent::none && result.value != output) {
            ExportSettings changed = settings;
            changed.output = result.value;
            change(ui, std::move(changed));
        }
    });
}

std::string copied_items(const ExportPlan& plan) {
    std::string items;
    for (const ExportStep& step : plan.steps) {
        if (!items.empty()) {
            items += "  ";
        }
        items += step.to.filename().string();
    }
    return items;
}

void problem_list(const Theme& theme, const ExportPlan& plan) {
    for (const std::string& problem : plan.problems) {
        wrapped_text(theme, view_of(problem), size::small_text, color::error);
    }
}

void copied_list(Ui& ui, const ExportPlan& plan) {
    const Theme& theme = ui.theme();
    text(theme, ui.keep(std::format("Copies {} items:", plan.steps.size())), size::small_text,
         color::text_muted);
    wrapped_text(theme, ui.keep(copied_items(plan)), size::small_text, color::text_soft);
    text(theme, CLAY_STRING("and a README.txt telling how to run every mode."), size::small_text,
         color::text_muted);
}

void plan_summary(Ui& ui, const ExportPlan& plan) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("ExportPlan"),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = CLAY_PADDING_ALL(theme.px16(8.F)),
                     .childGap = theme.px16(4.F),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::field,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius))}) {
        if (plan.problems.empty()) {
            copied_list(ui, plan);
        } else {
            problem_list(theme, plan);
        }
    }
}

void actions_row(Ui& ui, const ExportPlan& plan) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}, .childGap = ui.theme().px16(8.F)}}) {
        spacer();
        if (button(ui, CLAY_ID("ExportCancel"), CLAY_STRING("Cancel"), secondary)) {
            ui.emit(command::CloseExport{});
        }
        if (button(ui, CLAY_ID("ExportRun"), CLAY_STRING("Export"), primary,
                   plan.problems.empty())) {
            ui.emit(command::RunExport{});
        }
    }
}

void dialog_body(Ui& ui, const ExportDialog& dialog) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("ExportDialog"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(dialog_width))},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        panel_header(theme, CLAY_STRING("EXPORT GAME"));
        CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                         .padding = CLAY_PADDING_ALL(theme.px16(size::gutter)),
                         .childGap = theme.px16(10.F),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
            name_row(ui, dialog.settings);
            output_row(ui, dialog.settings);
            plan_summary(ui, dialog.plan);
            actions_row(ui, dialog.plan);
        }
    }
}

} // namespace

void export_dialog(Ui& ui, const Editor& editor, Clay_Dimensions window) {
    const auto& dialog = editor.state().export_dialog;
    if (!dialog) {
        return;
    }
    if (!ui.widgets().wants_keyboard() && pressed_or_repeated(ui.input(), Key::escape)) {
        ui.emit(command::CloseExport{});
    }
    CLAY({.id = CLAY_ID("ExportBackdrop"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(window.width),
                                .height = CLAY_SIZING_FIXED(window.height)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = backdrop,
          .floating = {.zIndex = dialog_layer, .attachTo = CLAY_ATTACH_TO_ROOT}}) {
        dialog_body(ui, *dialog);
    }
}

} // namespace editor
