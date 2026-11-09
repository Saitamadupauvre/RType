#pragma once

#include <gtest/gtest.h>

#include <clay.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../FixedMeasure.hpp"
#include "clay/ClayContext.hpp"
#include "ui/Platform.hpp"
#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/UiInput.hpp"
#include "ui/WidgetState.hpp"

class WidgetFixture : public ::testing::Test {
protected:
    static constexpr Clay_Dimensions window{.width = 800, .height = 600};

    editor::ClayContext _clay{window, fixed_measure};
    editor::WidgetState _widgets;
    std::string _clipboard;
    editor::Platform _platform{
        .measure = fixed_measure,
        .read_clipboard = [this] { return _clipboard; },
        .write_clipboard = [this](std::string_view text) { _clipboard = std::string(text); },
        .texture_size = {}};

    template <typename Draw> void frame(const editor::UiInput& input, Draw&& draw) {
        editor::Ui ui(editor::Theme{}, input, _widgets, _platform);
        Clay_SetLayoutDimensions(window);
        Clay_SetPointerState(input.pointer, input.primary_down);
        Clay_BeginLayout();
        CLAY({.id = CLAY_ID("Root"),
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
            std::forward<Draw>(draw)(ui);
        }
        Clay_EndLayout();
        _widgets.end_frame();
    }

    static editor::UiInput click_at(Clay_ElementId id, float x_from_left = 1.F) {
        const Clay_BoundingBox box = Clay_GetElementData(id).boundingBox;
        return {.pointer = {.x = box.x + x_from_left, .y = box.y + (box.height / 2.F)},
                .primary_down = true,
                .primary_pressed = true};
    }

    static editor::UiInput click_outside() {
        return {.pointer = {.x = window.width - 1.F, .y = window.height - 1.F},
                .primary_down = true,
                .primary_pressed = true};
    }

    static editor::UiInput type(std::string text) { return {.typed = std::move(text)}; }

    static editor::UiInput press(editor::Key key, bool primary = false, bool shift = false) {
        return {.keys_pressed = {key}, .primary_modifier = primary, .shift = shift};
    }
};
