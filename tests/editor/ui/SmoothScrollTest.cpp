#include <gtest/gtest.h>

#include "FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/SmoothScroll.hpp"

namespace command = editor::command;

namespace {

constexpr float frame_time = 1.F / 60.F;
constexpr int settle_frames = 120;

class SmoothScroll : public FrameFixture {
protected:
    const Clay_ElementId _console = CLAY_ID("BottomContent");

    void SetUp() override {
        for (int line = 0; line < 60; ++line) {
            _editor.handle(command::Undo{});
        }
    }

    void TearDown() override { Clay_SetDebugModeEnabled(false); }

    editor::UiInput over(Clay_ElementId id, editor::Theme theme = {}) {
        frame({}, theme);
        const Clay_BoundingBox box = box_of(id);
        editor::UiInput input{.pointer = {.x = box.x + 10.F, .y = box.y + 10.F},
                              .delta_time = frame_time};
        frame(input, theme);
        return input;
    }

    void scroll(editor::UiInput input, float notches, editor::Theme theme = {}) {
        input.wheel = {.x = 0.F, .y = notches};
        frame(input, theme);
    }

    void settle(const editor::UiInput& input, editor::Theme theme = {}) {
        for (int index = 0; index < settle_frames; ++index) {
            frame(input, theme);
        }
    }

    static float position(Clay_ElementId id) {
        const Clay_ScrollContainerData data = Clay_GetScrollContainerData(id);
        EXPECT_TRUE(data.found);
        return data.found ? data.scrollPosition->y : 0.F;
    }

    static float end_of(Clay_ElementId id) {
        const Clay_ScrollContainerData data = Clay_GetScrollContainerData(id);
        return data.scrollContainerDimensions.height - data.contentDimensions.height;
    }
};

} // namespace

TEST_F(SmoothScroll, WheelEasesTowardTheTarget) {
    const editor::UiInput input = over(_console);
    scroll(input, -1.F);

    EXPECT_LT(position(_console), 0.F);
    EXPECT_GT(position(_console), -editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, SettlesExactlyOneStepPerNotch) {
    const editor::UiInput input = over(_console);
    scroll(input, -1.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(_console), -editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, NotchesAddUp) {
    const editor::UiInput input = over(_console);
    scroll(input, -1.F);
    scroll(input, -1.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(_console), -2.F * editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, ZeroDeltaTimeKeepsThePositionButNotTheTarget) {
    editor::UiInput input = over(_console);
    input.delta_time = 0.F;
    scroll(input, -1.F);
    EXPECT_FLOAT_EQ(position(_console), 0.F);

    input.delta_time = frame_time;
    settle(input);
    EXPECT_FLOAT_EQ(position(_console), -editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, StopsAtTheEndOfTheContent) {
    const editor::UiInput input = over(_console);
    scroll(input, -1000.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(_console), end_of(_console));
}

TEST_F(SmoothScroll, StopsAtTheTop) {
    const editor::UiInput input = over(_console);
    scroll(input, 5.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(_console), 0.F);
}

TEST_F(SmoothScroll, OnlyTheHoveredContainerScrolls) {
    const editor::UiInput input = over(CLAY_ID("Viewport"));
    scroll(input, -1.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(_console), 0.F);
}

TEST_F(SmoothScroll, StepFollowsTheUiScale) {
    const editor::Theme theme{.scale = 2.F};
    const editor::UiInput input = over(_console, theme);
    scroll(input, -1.F, theme);
    settle(input, theme);

    EXPECT_FLOAT_EQ(position(_console), -2.F * editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, ScrollsTheClayDebugView) {
    Clay_SetDebugModeEnabled(true);
    const Clay_ElementId pane = CLAY_ID("Clay__DebugViewOuterScrollPane");
    const editor::UiInput input = over(pane);
    scroll(input, -1.F);
    settle(input);

    EXPECT_FLOAT_EQ(position(pane), -editor::SmoothScroll::step);
}

TEST_F(SmoothScroll, PrefabFilterStaysAboveTheScrolledList) {
    frame();
    const Clay_BoundingBox filter = box_of(CLAY_ID("PrefabFilter"));
    const Clay_BoundingBox list = box_of(CLAY_ID("Prefabs"));

    EXPECT_LE(filter.y + filter.height, list.y);
}
