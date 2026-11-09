#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/sections/Overlays.hpp"

using editor::BottomTab;
using editor::UiInput;

namespace command = editor::command;

namespace {

constexpr std::string_view refused_text = "nothing to undo";

class Overlays : public FrameFixture {
protected:
    bool toast_shown(UiInput input = {}) {
        const auto texts = frame(input).texts;
        const auto logged = std::ranges::count(_editor.state().console, std::string(refused_text),
                                               &editor::ConsoleLine::text);
        return std::ranges::count(texts, refused_text) == logged + 1;
    }
};

} // namespace

TEST_F(Overlays, ARefusalShowsOverTheViewport) {
    _editor.handle(command::Undo{});

    EXPECT_TRUE(toast_shown());
}

TEST_F(Overlays, TheRefusalFadesAfterAFewSeconds) {
    _editor.handle(command::Undo{});
    frame();

    EXPECT_FALSE(toast_shown(UiInput{.delta_time = editor::toast_seconds}));
}

TEST_F(Overlays, ARepeatedRefusalShowsAgain) {
    _editor.handle(command::Undo{});
    frame();
    ASSERT_FALSE(toast_shown(UiInput{.delta_time = editor::toast_seconds}));

    _editor.handle(command::Undo{});

    EXPECT_TRUE(toast_shown());
}

TEST_F(Overlays, DraggingAnAssetShowsItNextToThePointer) {
    _game.write("assets/sprites/ship.png", "");
    _editor.handle(command::ShowTab{.tab = BottomTab::assets});
    click(CLAY_IDI("AssetRow", 0));

    const auto texts = frame({.pointer = {.x = 300, .y = 200}, .primary_down = true}).texts;

    EXPECT_NE(std::ranges::find(texts, "sprites/ship.png"), texts.end());
    const Clay_BoundingBox box = box_of(CLAY_ID("DragChip"));
    EXPECT_GT(box.x, 300.F);
    EXPECT_GT(box.y, 200.F);
}

TEST_F(Overlays, DraggingAPrefabShowsItsName) {
    click(CLAY_IDI("PrefabRow", 0));

    frame({.pointer = {.x = 300, .y = 200}, .primary_down = true});

    EXPECT_TRUE(Clay_GetElementData(CLAY_ID("DragChip")).found);
}

TEST_F(Overlays, NoChipWithoutADrag) {
    frame();

    EXPECT_FALSE(Clay_GetElementData(CLAY_ID("DragChip")).found);
}
