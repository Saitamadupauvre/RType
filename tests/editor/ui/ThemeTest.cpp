#include <gtest/gtest.h>

#include "ui/Theme.hpp"

using editor::Theme;

TEST(Theme, ScaleOneKeepsDesignSizes) {
    const Theme theme{.scale = 1.F};
    EXPECT_FLOAT_EQ(theme.px(260.F), 260.F);
    EXPECT_EQ(theme.px16(13.F), 13);
}

TEST(Theme, SizesFollowTheScale) {
    const Theme theme{.scale = 2.F};
    EXPECT_FLOAT_EQ(theme.px(260.F), 520.F);
    EXPECT_EQ(theme.px16(13.F), 26);
}

TEST(Theme, IntegerSizesRoundToTheNearestPixel) {
    const Theme theme{.scale = 1.5F};
    EXPECT_EQ(theme.px16(1.F), 2);
    EXPECT_EQ(theme.px16(13.F), 20);
}
