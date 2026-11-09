#include <gtest/gtest.h>

#include "render/Geometry.hpp"

using editor::intersect;
using editor::roundness;

namespace {

void expect_box(Clay_BoundingBox actual, Clay_BoundingBox expected) {
    EXPECT_FLOAT_EQ(actual.x, expected.x);
    EXPECT_FLOAT_EQ(actual.y, expected.y);
    EXPECT_FLOAT_EQ(actual.width, expected.width);
    EXPECT_FLOAT_EQ(actual.height, expected.height);
}

} // namespace

TEST(Intersect, OverlappingBoxesKeepTheSharedArea) {
    expect_box(intersect({.x = 0, .y = 0, .width = 100, .height = 100},
                         {.x = 50, .y = 25, .width = 100, .height = 100}),
               {.x = 50, .y = 25, .width = 50, .height = 75});
}

TEST(Intersect, ContainedBoxIsKept) {
    expect_box(intersect({.x = 0, .y = 0, .width = 100, .height = 100},
                         {.x = 10, .y = 20, .width = 30, .height = 40}),
               {.x = 10, .y = 20, .width = 30, .height = 40});
}

TEST(Intersect, DisjointBoxesGiveAnEmptyBox) {
    const Clay_BoundingBox result = intersect({.x = 0, .y = 0, .width = 10, .height = 10},
                                              {.x = 50, .y = 50, .width = 10, .height = 10});
    EXPECT_FLOAT_EQ(result.width, 0.F);
    EXPECT_FLOAT_EQ(result.height, 0.F);
}

TEST(Roundness, RadiusIsRelativeToHalfTheShortSide) {
    EXPECT_FLOAT_EQ(roundness(5.F, 100.F, 20.F), 0.5F);
}

TEST(Roundness, RadiusLargerThanHalfTheSideIsClamped) {
    EXPECT_FLOAT_EQ(roundness(50.F, 100.F, 20.F), 1.F);
}

TEST(Roundness, EmptyBoxHasNoRoundness) { EXPECT_FLOAT_EQ(roundness(5.F, 0.F, 20.F), 0.F); }

TEST(Roundness, NegativeRadiusHasNoRoundness) {
    EXPECT_FLOAT_EQ(roundness(-5.F, 100.F, 20.F), 0.F);
}
