#include <gtest/gtest.h>

#include "../FrameFixture.hpp"
#include "ui/Layout.hpp"
#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/sections/BottomPanel.hpp"

using editor::bottom_panel_height;
using editor::Cursor;
using editor::Theme;
using editor::UiInput;

namespace {

class BottomPanelResize : public FrameFixture {
protected:
    void SetUp() override { frame(); }

    [[nodiscard]] static Clay_Vector2 grip_center() {
        const Clay_BoundingBox box = box_of(CLAY_ID("BottomPanelGrip"));
        return {.x = box.x + (box.width / 2.F), .y = box.y + (box.height / 2.F)};
    }

    void drag_grip_by(float dy) {
        const Clay_Vector2 start = grip_center();
        frame({.pointer = start, .primary_down = true, .primary_pressed = true});
        frame({.pointer = {.x = start.x, .y = start.y + dy}, .primary_down = true});
        frame({.pointer = {.x = start.x, .y = start.y + dy}});
        frame();
    }

    [[nodiscard]] static float panel_height() { return box_of(CLAY_ID("BottomPanel")).height; }

    Cursor cursor_with(const UiInput& input) {
        editor::Ui ui(Theme{}, input, _widgets, _platform);
        editor::build_frame(ui, _editor, window);
        return ui.cursor();
    }
};

} // namespace

TEST_F(BottomPanelResize, DraggingTheGripUpMakesThePanelTaller) {
    drag_grip_by(-50.F);

    EXPECT_FLOAT_EQ(panel_height(), 200.F);
}

TEST_F(BottomPanelResize, DraggingDownMakesItShorter) {
    drag_grip_by(40.F);

    EXPECT_FLOAT_EQ(panel_height(), 110.F);
}

TEST_F(BottomPanelResize, StaysBetweenHalfAndTwiceItsDesignHeight) {
    drag_grip_by(-500.F);
    EXPECT_FLOAT_EQ(panel_height(), 300.F);

    drag_grip_by(500.F);
    EXPECT_FLOAT_EQ(panel_height(), 75.F);
}

TEST_F(BottomPanelResize, DoubleClickingTheGripResetsIt) {
    drag_grip_by(-50.F);

    frame({.pointer = grip_center(),
           .primary_down = true,
           .primary_pressed = true,
           .click_count = 2});
    frame();

    EXPECT_FLOAT_EQ(panel_height(), 150.F);
}

TEST_F(BottomPanelResize, TheGripAsksForTheResizeCursor) {
    EXPECT_EQ(cursor_with({.pointer = grip_center()}), Cursor::resize_vertical);
    EXPECT_EQ(cursor_with({.pointer = {.x = 5.F, .y = 5.F}}), Cursor::standard);
}

TEST(BottomPanelHeight, KeepsTheChosenHeightWithinItsBounds) {
    EXPECT_FLOAT_EQ(bottom_panel_height(Theme{}, 900.F, 220.F), 220.F);
    EXPECT_FLOAT_EQ(bottom_panel_height(Theme{}, 900.F, 10.F), 75.F);
    EXPECT_FLOAT_EQ(bottom_panel_height(Theme{}, 900.F, 1000.F), 300.F);
}

TEST(BottomPanelHeight, LeavesTheWorkspaceItsMinimum) {
    EXPECT_FLOAT_EQ(bottom_panel_height(Theme{.scale = 2.F}, 800.F, 300.F), 328.F);
}
