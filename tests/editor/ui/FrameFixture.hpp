#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "../GameFixture.hpp"
#include "action/Action.hpp"
#include "action/Editor.hpp"
#include "clay/ClayContext.hpp"
#include "ui/Layout.hpp"
#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/UiInput.hpp"

inline Clay_Dimensions fixed_measure(std::string_view text, const Clay_TextElementConfig& config) {
    return {.width = static_cast<float>(text.size()) * static_cast<float>(config.fontSize) / 2.F,
            .height = static_cast<float>(config.fontSize)};
}

inline Clay_BoundingBox box_of(Clay_ElementId id) {
    const Clay_ElementData data = Clay_GetElementData(id);
    EXPECT_TRUE(data.found);
    return data.boundingBox;
}

struct Frame {
    std::vector<editor::Action> actions;
    std::vector<std::string> texts;
};

class FrameFixture : public GameFixture {
protected:
    static constexpr Clay_Dimensions window{.width = 1440, .height = 900};

    editor::Editor _editor{open_level()};
    editor::ClayContext _clay{window, fixed_measure};

    Frame frame(editor::UiInput input = {}, editor::Theme theme = {},
                Clay_Dimensions size = window) {
        editor::Ui ui(theme, input);
        const Clay_RenderCommandArray commands = editor::build_frame(ui, _editor, size);
        return {.actions = ui.take_actions(), .texts = texts_of(commands)};
    }

    std::vector<editor::Action> click(Clay_ElementId id) {
        frame();
        const Clay_BoundingBox box = box_of(id);
        return frame({.pointer = {.x = box.x + (box.width / 2.F), .y = box.y + (box.height / 2.F)},
                      .primary_down = true,
                      .primary_pressed = true})
            .actions;
    }

    [[nodiscard]] bool shows(std::string_view text) {
        const auto texts = frame().texts;
        return std::ranges::find(texts, text) != texts.end();
    }

    template <typename Wanted>
    static const Wanted* only(const std::vector<editor::Action>& actions) {
        if (actions.size() != 1) {
            return nullptr;
        }
        const auto* command = std::get_if<editor::Command>(&actions.front());
        return command ? std::get_if<Wanted>(command) : nullptr;
    }

private:
    static std::vector<std::string> texts_of(Clay_RenderCommandArray commands) {
        std::vector<std::string> texts;
        for (std::int32_t index = 0; index < commands.length; ++index) {
            const Clay_RenderCommand& command = *Clay_RenderCommandArray_Get(&commands, index);
            if (command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
                const Clay_StringSlice slice = command.renderData.text.stringContents;
                texts.emplace_back(slice.chars, static_cast<std::size_t>(slice.length));
            }
        }
        return texts;
    }
};
