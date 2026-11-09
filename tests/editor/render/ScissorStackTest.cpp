#include <gtest/gtest.h>

#include "render/ScissorStack.hpp"

using editor::ScissorStack;

TEST(ScissorStack, FirstPushIsActiveAsGiven) {
    ScissorStack stack;
    const Clay_BoundingBox active = stack.push({.x = 10, .y = 20, .width = 30, .height = 40});
    EXPECT_FLOAT_EQ(active.x, 10.F);
    EXPECT_FLOAT_EQ(active.width, 30.F);
}

TEST(ScissorStack, NestedPushIsClippedByItsParent) {
    ScissorStack stack;
    stack.push({.x = 0, .y = 0, .width = 100, .height = 100});
    const Clay_BoundingBox active = stack.push({.x = 50, .y = 50, .width = 100, .height = 100});
    EXPECT_FLOAT_EQ(active.width, 50.F);
    EXPECT_FLOAT_EQ(active.height, 50.F);
}

TEST(ScissorStack, PopRestoresTheParent) {
    ScissorStack stack;
    stack.push({.x = 0, .y = 0, .width = 100, .height = 100});
    stack.push({.x = 50, .y = 50, .width = 10, .height = 10});
    const auto restored = stack.pop();
    ASSERT_TRUE(restored.has_value());
    EXPECT_FLOAT_EQ(restored->width, 100.F);
}

TEST(ScissorStack, PoppingTheLastBoxDisablesClipping) {
    ScissorStack stack;
    stack.push({.x = 0, .y = 0, .width = 100, .height = 100});
    EXPECT_FALSE(stack.pop().has_value());
}

TEST(ScissorStack, PoppingAnEmptyStackDisablesClipping) {
    ScissorStack stack;
    EXPECT_FALSE(stack.pop().has_value());
}
