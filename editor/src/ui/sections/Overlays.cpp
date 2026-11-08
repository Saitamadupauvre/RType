#include "ui/sections/Overlays.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "ui/Text.hpp"
#include "viewport/ViewportState.hpp"

namespace editor {

namespace {

constexpr std::int16_t toast_layer = 150;
constexpr std::int16_t chip_layer = 250;
constexpr float chip_offset = 14.F;
constexpr float toast_margin = 16.F;
constexpr Clay_Color toast_background{.r = 90, .g = 24, .b = 28, .a = 235};

void chip(const Theme& theme, Clay_Vector2 pointer, std::string_view label) {
    CLAY({.id = CLAY_ID("DragChip"),
          .layout = {.padding = {.left = theme.px16(8.F),
                                 .right = theme.px16(8.F),
                                 .top = theme.px16(4.F),
                                 .bottom = theme.px16(4.F)}},
          .backgroundColor = color::accent,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .floating = {.offset = {.x = pointer.x + theme.px(chip_offset),
                                  .y = pointer.y + theme.px(chip_offset)},
                       .zIndex = chip_layer,
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                       .attachTo = CLAY_ATTACH_TO_ROOT}}) {
        text(theme, view_of(label), size::small_text, color::text_on_accent);
    }
}

} // namespace

void refusal_toast(Ui& ui, const Editor& editor) {
    ToastState& toast = ui.widgets().toast();
    const Refusal& refusal = editor.state().refusal;
    if (refusal.count != toast.shown) {
        toast = {.shown = refusal.count, .remaining = toast_seconds};
    }
    toast.remaining = std::max(0.F, toast.remaining - ui.input().delta_time);
    if (toast.remaining <= 0.F || refusal.text.empty()) {
        return;
    }
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("RefusalToast"),
          .layout = {.padding = CLAY_PADDING_ALL(theme.px16(10.F))},
          .backgroundColor = toast_background,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .floating = {.offset = {.x = 0.F, .y = -theme.px(toast_margin)},
                       .parentId = CLAY_ID("Viewport").id,
                       .zIndex = toast_layer,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                        .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                       .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID},
          .border = {.color = color::error, .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        text(theme, view_of(refusal.text), size::text, color::error);
    }
}

void drag_chip(Ui& ui) {
    const ViewportState& state = ui.widgets().viewport();
    const Clay_Vector2 pointer = ui.input().pointer;
    if (state.asset_drag) {
        chip(ui.theme(), pointer, state.asset_drag->path);
    } else if (state.placing && !state.placing->armed) {
        chip(ui.theme(), pointer, state.placing->type);
    }
}

} // namespace editor
