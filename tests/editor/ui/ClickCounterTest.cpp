#include <gtest/gtest.h>

#include "ui/ClickCounter.hpp"

using editor::ClickCounter;

TEST(ClickCounter, FirstPressIsASingleClick) {
    ClickCounter counter;
    EXPECT_EQ(counter.press({.x = 10, .y = 10}, 1.0), 1U);
}

TEST(ClickCounter, QuickPressesAtTheSamePlaceCountUp) {
    ClickCounter counter;
    counter.press({.x = 10, .y = 10}, 1.0);
    EXPECT_EQ(counter.press({.x = 11, .y = 10}, 1.2), 2U);
    EXPECT_EQ(counter.press({.x = 11, .y = 11}, 1.4), 3U);
}

TEST(ClickCounter, SlowPressStartsANewSeries) {
    ClickCounter counter;
    counter.press({.x = 10, .y = 10}, 1.0);
    EXPECT_EQ(counter.press({.x = 10, .y = 10}, 1.5), 1U);
}

TEST(ClickCounter, DistantPressStartsANewSeries) {
    ClickCounter counter;
    counter.press({.x = 10, .y = 10}, 1.0);
    EXPECT_EQ(counter.press({.x = 30, .y = 10}, 1.1), 1U);
}

TEST(ClickCounter, ClockGoingBackStartsANewSeries) {
    ClickCounter counter;
    counter.press({.x = 10, .y = 10}, 2.0);
    EXPECT_EQ(counter.press({.x = 10, .y = 10}, 1.9), 1U);
}

TEST(ClickCounter, CountRestartsInsteadOfOverflowing) {
    ClickCounter counter;
    for (int index = 0; index < 255; ++index) {
        counter.press({.x = 10, .y = 10}, 1.0);
    }
    EXPECT_EQ(counter.press({.x = 10, .y = 10}, 1.0), 1U);
}
