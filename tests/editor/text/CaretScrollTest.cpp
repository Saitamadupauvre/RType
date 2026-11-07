#include <gtest/gtest.h>

#include "text/CaretScroll.hpp"

using editor::scroll_to_caret;

TEST(CaretScroll, ShortTextNeverScrolls) {
    EXPECT_EQ(scroll_to_caret(0.F, 40.F, 50.F, 100.F), 0.F);
    EXPECT_EQ(scroll_to_caret(30.F, 40.F, 50.F, 100.F), 0.F);
}

TEST(CaretScroll, VisibleCaretKeepsTheScroll) {
    EXPECT_EQ(scroll_to_caret(100.F, 150.F, 400.F, 100.F), 100.F);
}

TEST(CaretScroll, CaretPastTheRightEdgeScrollsJustEnough) {
    EXPECT_EQ(scroll_to_caret(0.F, 250.F, 400.F, 100.F), 150.F);
}

TEST(CaretScroll, CaretBeforeTheLeftEdgeScrollsBack) {
    EXPECT_EQ(scroll_to_caret(200.F, 120.F, 400.F, 100.F), 120.F);
    EXPECT_EQ(scroll_to_caret(200.F, 0.F, 400.F, 100.F), 0.F);
}

TEST(CaretScroll, NeverScrollsPastTheEndOfTheText) {
    EXPECT_EQ(scroll_to_caret(350.F, 360.F, 400.F, 100.F), 300.F);
}

TEST(CaretScroll, FieldWithoutRoomFollowsTheCaret) {
    EXPECT_EQ(scroll_to_caret(0.F, 80.F, 100.F, 0.F), 80.F);
    EXPECT_EQ(scroll_to_caret(0.F, 80.F, 100.F, -5.F), 80.F);
}
