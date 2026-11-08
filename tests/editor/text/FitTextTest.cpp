#include <gtest/gtest.h>

#include <string_view>

#include "text/FitText.hpp"

namespace {

float ten_per_byte(std::string_view text) { return static_cast<float>(text.size()) * 10.F; }

} // namespace

TEST(FitText, KeepsATextThatFits) {
    EXPECT_EQ(editor::fit_text("hello", 50.F, ten_per_byte), "hello");
}

TEST(FitText, ShortensWithAnEllipsis) {
    EXPECT_EQ(editor::fit_text("hello world", 60.F, ten_per_byte), "hel...");
}

TEST(FitText, NeverCutsInsideACodePoint) {
    EXPECT_EQ(editor::fit_text("\xc3\xa9\xc3\xa9\xc3\xa9", 50.F, ten_per_byte), "\xc3\xa9...");
}

TEST(FitText, OnlyTheEllipsisWhenNothingFits) {
    EXPECT_EQ(editor::fit_text("hello", 5.F, ten_per_byte), "...");
}
