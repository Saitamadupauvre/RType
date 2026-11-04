#include <gtest/gtest.h>

#include <string_view>

#include "clay/ClayContext.hpp"
#include "ui/Layout.hpp"
#include "ui/Theme.hpp"

using editor::build_layout;
using editor::ClayContext;
using editor::Theme;

namespace {

Clay_Dimensions fixed_measure(std::string_view text, const Clay_TextElementConfig& config) {
    return {.width = static_cast<float>(text.size()) * static_cast<float>(config.fontSize) / 2.F,
            .height = static_cast<float>(config.fontSize)};
}

Clay_BoundingBox box_of(Clay_ElementId id) {
    const Clay_ElementData data = Clay_GetElementData(id);
    EXPECT_TRUE(data.found);
    return data.boundingBox;
}

class LayoutAtScale : public testing::TestWithParam<float> {};

} // namespace

TEST(Layout, BuildsWithoutClayErrors) {
    const ClayContext clay({1440, 900}, fixed_measure);
    build_layout(Theme{}, {1440, 900});
    EXPECT_TRUE(clay.errors().empty());
}

TEST(Layout, PanelsKeepTheirDesignSizes) {
    const ClayContext clay({1440, 900}, fixed_measure);
    build_layout(Theme{}, {1440, 900});

    EXPECT_FLOAT_EQ(box_of(CLAY_ID("MenuBar")).height, 36.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Toolbar")).height, 40.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Sidebar")).width, 260.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Inspector")).width, 340.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("BottomPanel")).height, 150.F);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("StatusBar")).height, 24.F);
}

TEST(Layout, ViewportTakesTheRemainingSpace) {
    const ClayContext clay({1440, 900}, fixed_measure);
    build_layout(Theme{}, {1440, 900});

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_FLOAT_EQ(viewport.x, 260.F);
    EXPECT_FLOAT_EQ(viewport.y, 76.F);
    EXPECT_FLOAT_EQ(viewport.width, 840.F);
    EXPECT_FLOAT_EQ(viewport.height, 650.F);
}

TEST(Layout, ViewportFollowsTheWindowSize) {
    const ClayContext clay({1920, 1080}, fixed_measure);
    build_layout(Theme{}, {1920, 1080});

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_FLOAT_EQ(viewport.width, 1320.F);
    EXPECT_FLOAT_EQ(viewport.height, 830.F);
}

TEST(Layout, TinyWindowNeverGivesANegativeViewport) {
    const ClayContext clay({300, 200}, fixed_measure);
    build_layout(Theme{}, {300, 200});

    const Clay_BoundingBox viewport = box_of(CLAY_ID("Viewport"));
    EXPECT_GE(viewport.width, 0.F);
    EXPECT_GE(viewport.height, 0.F);
}

TEST_P(LayoutAtScale, PanelsScaleAndTheViewportKeepsTheRest) {
    const float scale = GetParam();
    const Clay_Dimensions window{.width = 1440.F * scale, .height = 900.F * scale};
    const ClayContext clay(window, fixed_measure);
    build_layout(Theme{.scale = scale}, window);

    EXPECT_TRUE(clay.errors().empty());
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Sidebar")).width, 260.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Inspector")).width, 340.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("MenuBar")).height, 36.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Viewport")).width, 840.F * scale);
    EXPECT_FLOAT_EQ(box_of(CLAY_ID("Viewport")).height, 650.F * scale);
}

INSTANTIATE_TEST_SUITE_P(Layout, LayoutAtScale, testing::Values(1.F, 1.5F, 2.F));
