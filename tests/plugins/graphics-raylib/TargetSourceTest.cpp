#include <gtest/gtest.h>

#include "TargetSource.hpp"

using engine::core::Vec2;
using engine::graphics::Rect;

TEST(TargetSource, FlipsTheWholeTargetUpright) {
    const Rect source = upright_target_source({.width = 320, .height = 180}, Vec2{320, 180});

    EXPECT_EQ(source, (Rect{.x = 0, .y = 0, .width = 320, .height = -180}));
}

TEST(TargetSource, MapsATopRegionToTheBottomRowsOfTheTexture) {
    const Rect source =
        upright_target_source({.x = 10, .y = 0, .width = 50, .height = 40}, Vec2{320, 180});

    EXPECT_EQ(source, (Rect{.x = 10, .y = 140, .width = 50, .height = -40}));
}

TEST(TargetSource, KeepsAMirroredRegionMirrored) {
    const Rect source =
        upright_target_source({.x = 0, .y = 20, .width = 100, .height = -60}, Vec2{320, 180});

    EXPECT_EQ(source, (Rect{.x = 0, .y = 100, .width = 100, .height = 60}));
}

TEST(TargetSource, LeavesAHorizontalMirrorAlone) {
    const Rect source =
        upright_target_source({.x = 0, .y = 0, .width = -320, .height = 180}, Vec2{320, 180});

    EXPECT_EQ(source.width, -320);
    EXPECT_EQ(source.height, -180);
}
