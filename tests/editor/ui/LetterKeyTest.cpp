#include <gtest/gtest.h>

#include <optional>

#include "ui/LetterKey.hpp"

using editor::Key;
using editor::letter_key;

TEST(LetterKey, MapsTheLettersTheEditorUses) {
    EXPECT_EQ(letter_key("a"), Key::a);
    EXPECT_EQ(letter_key("z"), Key::z);
    EXPECT_EQ(letter_key("h"), Key::h);
}

TEST(LetterKey, IgnoresCase) { EXPECT_EQ(letter_key("A"), Key::a); }

TEST(LetterKey, RejectsLettersTheEditorDoesNotUse) { EXPECT_EQ(letter_key("q"), std::nullopt); }

TEST(LetterKey, RejectsEmptyAndLongerNames) {
    EXPECT_EQ(letter_key(""), std::nullopt);
    EXPECT_EQ(letter_key("ab"), std::nullopt);
}

TEST(LetterKey, RejectsNonAsciiNames) { EXPECT_EQ(letter_key("\xC3\xA9"), std::nullopt); }
