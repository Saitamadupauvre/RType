#include <gtest/gtest.h>

#include "FrameFixture.hpp"
#include "ui/Theme.hpp"

using editor::Theme;

namespace {

using Layout = FrameFixture;

class LayoutAtScale : public FrameFixture, public testing::WithParamInterface<float> {};

} // namespace

TEST_F(Layout, BuildsWithoutClayErrors) {
    frame();
    EXPECT_TRUE(_clay.errors().empty());
}

TEST_F(Layout, PanelsKeepTheirDesignSizes) {
    frame();

    EXPECT_FLOAT_EQ(box_of(CLAY_ID("MenuBar")).height, 36.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Toolbar")).height, 40.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Sidebar")).width, 260.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Inspector")).width, 340.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("BottomPanel")).height, 150.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("StatusBar")).height, 24.F);
}

TEST_F(Layout, ViewportTakesTheRemainingSpace) {
    frame();

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_FLOAT_EQ(viewport.x, 260.F);
    EXPECT_FLOAT_EQ(viewport.y, 76.F);
    EXPECT_FLOAT_EQ(viewport.width, 840.F);
    EXPECT_FLOAT_EQ(viewport.height, 650.F);
}

TEST_F(Layout, ViewportFollowsTheWindowSize) {
    frame({}, {}, {.width = 1920, .height = 1080});

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_FLOAT_EQ(viewport.width, 1320.F);
    EXPECT_FLOAT_EQ(viewport.height, 830.F);
}

TEST_F(Layout, TinyWindowNeverGivesANegativeViewport) {
    frame({}, {}, {.width = 300, .height = 200});

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_GE(viewport.width, 0.F);
    EXPECT_GE(viewport.height, 0.F);
}

TEST_P(LayoutAtScale, PanelsScaleAndTheViewportKeepsTheRest) {
    const float scale = GetParam();
    frame({}, Theme{.scale = scale}, {.width = 1440.F * scale, .height = 900.F * scale});

    EXPECT_TRUE(_clay.errors().empty());
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Sidebar")).width, 260.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Inspector")).width, 340.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("MenuBar")).height, 36.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Viewport")).width, 840.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Viewport")).height, 650.F * scale);
}

INSTANTIATE_TEST_SUITE_P(Layout, LayoutAtScale, testing::Values(1.F, 1.5F, 2.F));

TEST_F(Layout, ShortWindowKeepsTheConsoleAndTheStatusBarVisible) {
    _editor.handle(editor::command::Select{.id = id_of(_editor.session(), "ship")});
    const Clay_Dimensions size{.width = 1440.F, .height = 300.F};
    frame({}, {}, size);
    frame({}, {}, size);

    const Clay_BoundingBox status = box_of(CLAY_ID("StatusBar"));
    EXPECT_LE(status.y + status.height, size.height + 0.5F);
    EXPECT_GE(box_of(CLAY_ID("BottomPanel")).height, 60.F);
}

TEST_F(Layout, InspectorScrollsWhenItsFieldsDoNotFit) {
    _editor.handle(editor::command::Select{.id = id_of(_editor.session(), "ship")});
    const Clay_Dimensions size{.width = 1440.F, .height = 300.F};
    frame({}, {}, size);
    frame({}, {}, size);

    const Clay_ScrollContainerData body = Clay_GetScrollContainerData(CLAY_ID("InspectorBody"));
    ASSERT_TRUE(body.found);
    EXPECT_GT(body.contentDimensions.height, body.scrollContainerDimensions.height);
}

TEST_F(Layout, SelectionAndDebugViewStayWithinClayLimits) {
    _editor.handle(editor::command::Select{.id = id_of(_editor.session(), "ship")});
    Clay_SetDebugModeEnabled(true);
    frame();
    frame();
    Clay_SetDebugModeEnabled(false);

    EXPECT_TRUE(_clay.errors().empty());
}
