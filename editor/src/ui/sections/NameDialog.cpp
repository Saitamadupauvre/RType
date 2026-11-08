#include "ui/sections/NameDialog.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "ui/Text.hpp"
#include "ui/UiInput.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/Button.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

constexpr std::int16_t dialog_layer = 200;
constexpr std::int16_t field_layer = dialog_layer + 1;
constexpr float dialog_width = 420.F;
constexpr Clay_Color backdrop{.r = 0, .g = 0, .b = 0, .a = 150};
constexpr ButtonStyle primary{.background = color::accent, .foreground = color::text_on_accent};
constexpr ButtonStyle secondary{.background = color::toolbar, .foreground = color::text};

Clay_ElementId field_id() { return CLAY_ID("NameDialogField"); }

Clay_ElementId panel_id() { return CLAY_ID("NameDialog"); }

struct Drawn {
    TextResult field;
    bool create = false;
    bool cancel = false;
};

Clay_String title(NewItem item) {
    return item == NewItem::prefab ? CLAY_STRING("NEW PREFAB") : CLAY_STRING("NEW SCENE");
}

Clay_String hint(NewItem item) {
    return item == NewItem::prefab ? CLAY_STRING("Writes scripts/<name>.lua and opens it.")
                                   : CLAY_STRING("Writes scenes/<name>.json and opens it.");
}

void actions_row(Ui& ui, Drawn& drawn) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()}, .childGap = ui.theme().px16(8.F)}}) {
        spacer();
        drawn.cancel = button(ui, CLAY_ID("NameDialogCancel"), CLAY_STRING("Cancel"), secondary);
        drawn.create = button(ui, CLAY_ID("NameDialogCreate"), CLAY_STRING("Create"), primary);
    }
}

Drawn draw(Ui& ui, const NameDialog& dialog, Clay_Dimensions window) {
    const Theme& theme = ui.theme();
    Drawn drawn;
    CLAY({.id = CLAY_ID("NameDialogBackdrop"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(window.width),
                                .height = CLAY_SIZING_FIXED(window.height)},
                     .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = backdrop,
          .floating = {.zIndex = dialog_layer, .attachTo = CLAY_ATTACH_TO_ROOT}}) {
        CLAY({.id = panel_id(),
              .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(dialog_width))},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .backgroundColor = color::panel,
              .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
              .border = {.color = color::border, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
            panel_header(theme, title(dialog.item));
            CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                             .padding = CLAY_PADDING_ALL(theme.px16(size::gutter)),
                             .childGap = theme.px16(10.F),
                             .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
                drawn.field = text_field(ui, field_id(), dialog.name,
                                         {.placeholder = "name", .layer = field_layer});
                text(theme, hint(dialog.item), size::small_text, color::text_muted);
                actions_row(ui, drawn);
            }
        }
    }
    return drawn;
}

void keep_focus(Ui& ui, const std::string& name) {
    WidgetState& widgets = ui.widgets();
    widgets.cancel_tab();
    if (!widgets.is_focused(field_id())) {
        widgets.focus(field_id(), name);
    }
}

Action create(NewItem item, std::string name) {
    if (item == NewItem::prefab) {
        return command::CreatePrefab{.name = std::move(name)};
    }
    return command::CreateScene{.name = std::move(name)};
}

void react(Ui& ui, const NameDialog& dialog, const Drawn& drawn, bool clicked_outside) {
    const FieldEvent event = drawn.field.event;
    const std::string& typed = event == FieldEvent::none ? dialog.name : drawn.field.value;
    if (event == FieldEvent::changed || (event == FieldEvent::committed && typed != dialog.name)) {
        ui.emit(command::SetNameDialogText{.name = typed});
    }
    if (drawn.cancel || clicked_outside || event == FieldEvent::cancelled) {
        ui.emit(command::CloseNameDialog{});
        return;
    }
    if (drawn.create ||
        (event == FieldEvent::committed && pressed_or_repeated(ui.input(), Key::enter))) {
        ui.emit(create(dialog.item, typed));
    }
    keep_focus(ui, typed);
}

} // namespace

void name_dialog(Ui& ui, const Editor& editor, Clay_Dimensions window) {
    const auto& dialog = editor.state().name_dialog;
    if (!dialog) {
        return;
    }
    const bool clicked_outside = ui.input().primary_pressed && !Ui::hovered(panel_id());
    const Drawn drawn = draw(ui, *dialog, window);
    react(ui, *dialog, drawn, clicked_outside);
}

} // namespace editor
