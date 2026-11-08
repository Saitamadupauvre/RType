#include "ui/widgets/TextField.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "text/CaretScroll.hpp"
#include "text/FitText.hpp"
#include "text/TextEdit.hpp"
#include "ui/Text.hpp"
#include "ui/widgets/FieldKeys.hpp"

namespace editor {

namespace {

constexpr float padding = 6.F;
constexpr float caret_width = 1.F;

Clay_TextElementConfig font_of(const Theme& theme, Face face) {
    Clay_TextElementConfig config{};
    config.fontId = static_cast<std::uint16_t>(face);
    config.fontSize = theme.px16(size::text);
    return config;
}

void place_caret_at(Ui& ui, Clay_ElementId id, float pointer_x, Face face, bool extend) {
    const Clay_ElementData box = Clay_GetElementData(id);
    const float x =
        pointer_x - box.boundingBox.x - ui.theme().px(padding) + ui.widgets().text_offset();
    const Clay_TextElementConfig font = font_of(ui.theme(), face);
    TextEdit& draft = ui.widgets().draft();
    const auto width = [&ui, &font](std::string_view prefix) {
        return ui.platform().measure(prefix, font).width;
    };
    draft.place_caret(caret_at(draft.text(), x, width), extend);
}

void take_requested_focus(Ui& ui, Clay_ElementId id, std::string_view value, Face face) {
    const std::optional<FocusRequest> request = ui.widgets().focus_requested(id);
    if (!request) {
        return;
    }
    ui.widgets().focus(id, value);
    if (request->click_x) {
        place_caret_at(ui, id, *request->click_x, face, false);
        ui.widgets().set_dragging(ui.input().primary_down);
    } else {
        ui.widgets().draft().select_all();
    }
}

void select_by_clicks(TextEdit& draft, std::uint8_t clicks) {
    if (clicks == 2) {
        draft.select_word();
    } else if (clicks >= 3) {
        draft.select_all();
    }
}

void apply_pointer(Ui& ui, Clay_ElementId id, Face face) {
    const UiInput& input = ui.input();
    WidgetState& widgets = ui.widgets();
    if (input.primary_pressed) {
        place_caret_at(ui, id, input.pointer.x, face, input.shift);
        select_by_clicks(widgets.draft(), input.click_count);
        widgets.set_dragging(input.click_count <= 1);
    } else if (widgets.dragging() && input.primary_down) {
        place_caret_at(ui, id, input.pointer.x, face, true);
    }
    if (!input.primary_down) {
        widgets.set_dragging(false);
    }
}

TextResult finish(Ui& ui, FieldEvent event, std::string value) {
    ui.widgets().blur();
    return {.event = event, .value = std::move(value)};
}

TextResult edit_focused(Ui& ui, Clay_ElementId id, Face face) {
    const UiInput& input = ui.input();
    TextEdit& draft = ui.widgets().draft();
    if (input.primary_pressed && !Ui::hovered(id)) {
        return finish(ui, FieldEvent::committed, draft.text());
    }
    apply_pointer(ui, id, face);
    if (pressed_or_repeated(input, Key::escape)) {
        return finish(ui, FieldEvent::cancelled, ui.widgets().original());
    }
    if (pressed_or_repeated(input, Key::enter)) {
        return finish(ui, FieldEvent::committed, draft.text());
    }
    if (pressed_or_repeated(input, Key::tab)) {
        ui.widgets().request_tab(input.shift);
        return {.event = FieldEvent::committed, .value = draft.text()};
    }
    const std::string before = draft.text();
    apply_editing_keys(input, ui.platform(), draft);
    if (draft.text() == before) {
        return {};
    }
    return {.event = FieldEvent::changed, .value = draft.text()};
}

void caret(const Theme& theme) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(caret_width)),
                                .height = CLAY_SIZING_FIXED(theme.px(size::text))}},
          .backgroundColor = color::focus}) {}
}

void piece(const Theme& theme, std::string_view content, Face face) {
    if (!content.empty()) {
        text(theme, view_of(content), size::text, color::text, face);
    }
}

void selected_piece(const Theme& theme, std::string_view content, Face face) {
    CLAY({.backgroundColor = color::selected}) { piece(theme, content, face); }
}

void draft_content(const Theme& theme, const TextEdit& draft, Face face) {
    const std::string_view all = draft.text();
    const std::size_t first = std::min(draft.caret(), draft.anchor());
    const std::size_t last = std::max(draft.caret(), draft.anchor());
    piece(theme, all.substr(0, first), face);
    if (draft.caret() == first) {
        caret(theme);
    }
    if (draft.has_selection()) {
        selected_piece(theme, all.substr(first, last - first), face);
    }
    if (draft.caret() != first) {
        caret(theme);
    }
    piece(theme, all.substr(last), face);
}

Clay_String fitted(Ui& ui, std::optional<float> field_width, std::string_view shown, Face face) {
    if (!field_width) {
        return view_of(shown);
    }
    const float width = *field_width - (2.F * ui.theme().px(padding));
    const Clay_TextElementConfig font = font_of(ui.theme(), face);
    return ui.keep(fit_text(shown, width, [&ui, &font](std::string_view prefix) {
        return ui.platform().measure(prefix, font).width;
    }));
}

void shown_text(Ui& ui, std::optional<float> field_width, std::string_view shown,
                const TextFieldOptions& options) {
    if (shown.empty()) {
        text(ui.theme(), view_of(options.placeholder), size::text, color::text_dim, options.face);
        return;
    }
    const Clay_Color color = options.read_only ? color::text_muted : color::text;
    text(ui.theme(), fitted(ui, field_width, shown, options.face), size::text, color, options.face);
}

void shown_content(Ui& ui, std::optional<float> field_width, std::string_view shown,
                   const TextFieldOptions& options) {
    CLAY({.floating = {.offset = {.x = ui.theme().px(padding), .y = 0.F},
                       .zIndex = options.layer,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER,
                                        .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                       .attachTo = CLAY_ATTACH_TO_PARENT,
                       .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT}}) {
        shown_text(ui, field_width, shown, options);
    }
}

float scrolled(Ui& ui, std::optional<float> field_width, Face face) {
    float& offset = ui.widgets().text_offset();
    if (!field_width) {
        return offset;
    }
    const TextEdit& draft = ui.widgets().draft();
    const Clay_TextElementConfig font = font_of(ui.theme(), face);
    const auto width = [&ui, &font](std::string_view prefix) {
        return ui.platform().measure(prefix, font).width;
    };
    const std::string_view all = draft.text();
    const float visible =
        *field_width - (2.F * ui.theme().px(padding)) - ui.theme().px(caret_width);
    offset = scroll_to_caret(offset, width(all.substr(0, draft.caret())), width(all), visible);
    return offset;
}

void draw(Ui& ui, Clay_ElementId id, std::string_view shown, const TextFieldOptions& options) {
    const Theme& theme = ui.theme();
    const bool focused = ui.widgets().is_focused(id);
    const Clay_ElementData previous = Clay_GetElementData(id);
    const std::optional<float> field_width = previous.found && previous.boundingBox.width > 0.F
                                                 ? std::optional<float>(previous.boundingBox.width)
                                                 : std::nullopt;
    CLAY({.id = id,
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(),
                                .height = CLAY_SIZING_FIXED(theme.px(size::row))},
                     .padding = {.left = theme.px16(padding), .right = theme.px16(padding)},
                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = color::field,
          .cornerRadius = CLAY_CORNER_RADIUS(theme.px(size::radius)),
          .clip = {.horizontal = focused,
                   .childOffset = {.x = focused ? -scrolled(ui, field_width, options.face) : 0.F,
                                   .y = 0.F}},
          .border = {.color = focused ? color::focus : color::border,
                     .width = CLAY_BORDER_OUTSIDE(theme.px16(1.F))}}) {
        if (focused) {
            draft_content(theme, ui.widgets().draft(), options.face);
        } else {
            shown_content(ui, field_width, shown, options);
        }
    }
}

std::string_view kept(Ui& ui, const std::string& value) {
    return {ui.keep(value).chars, value.size()};
}

} // namespace

TextResult text_field(Ui& ui, Clay_ElementId id, std::string_view value,
                      const TextFieldOptions& options) {
    TextResult result;
    if (!options.read_only) {
        ui.widgets().add_to_tab_order(id);
        take_requested_focus(ui, id, value, options.face);
        if (ui.widgets().is_focused(id)) {
            result = edit_focused(ui, id, options.face);
        } else if (ui.clicked(id)) {
            ui.widgets().request_focus(id, ui.input().pointer.x);
        }
    }
    draw(ui, id, result.event == FieldEvent::committed ? kept(ui, result.value) : value, options);
    return result;
}

} // namespace editor
