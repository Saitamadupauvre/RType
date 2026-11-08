#include <gtest/gtest.h>

#include "viewport/Snap.hpp"

using engine::core::Vec2;

TEST(Snap, RoundsToTheNearestGridLine) {
    EXPECT_EQ(editor::snap_to_grid({7.F, 9.F}, 16.F), Vec2(0.F, 16.F));
    EXPECT_EQ(editor::snap_to_grid({24.F, 40.F}, 16.F), Vec2(32.F, 48.F));
}

TEST(Snap, RoundsNegativeCoordinates) {
    EXPECT_EQ(editor::snap_to_grid({-7.F, -9.F}, 16.F), Vec2(0.F, -16.F));
}

TEST(Snap, NonPositiveGridKeepsThePoint) {
    EXPECT_EQ(editor::snap_to_grid({7.5F, 9.F}, 0.F), Vec2(7.5F, 9.F));
    EXPECT_EQ(editor::snap_to_grid({7.5F, 9.F}, -8.F), Vec2(7.5F, 9.F));
}

TEST(Snap, DropSnapsWhenSnapIsOn) {
    EXPECT_EQ(editor::drop_position({30.F, 30.F}, 32.F, true, false), Vec2(32.F, 32.F));
}

TEST(Snap, AltSkipsSnappingForThisDrop) {
    EXPECT_EQ(editor::drop_position({30.F, 30.F}, 32.F, true, true), Vec2(30.F, 30.F));
}

TEST(Snap, DropKeepsThePointWhenSnapIsOff) {
    EXPECT_EQ(editor::drop_position({30.F, 30.F}, 32.F, false, false), Vec2(30.F, 30.F));
}
