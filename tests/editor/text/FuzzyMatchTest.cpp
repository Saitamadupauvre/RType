#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

#include "text/FuzzyMatch.hpp"

using editor::fuzzy_rank;
using editor::fuzzy_score;

namespace {

constexpr std::array<std::string_view, 6> labels{
    "Save scene", "Select tool", "Show hitboxes", "Snap to grid", "Stop", "Undo",
};

} // namespace

TEST(FuzzyMatch, EmptyQueryMatchesEverything) {
    EXPECT_EQ(fuzzy_score("", "Save scene"), 0);
    EXPECT_EQ(fuzzy_rank("", labels), (std::vector<std::size_t>{0, 1, 2, 3, 4, 5}));
}

TEST(FuzzyMatch, CharactersMustAppearInOrder) {
    EXPECT_TRUE(fuzzy_score("sv", "Save scene").has_value());
    EXPECT_FALSE(fuzzy_score("vs", "Save").has_value());
    EXPECT_FALSE(fuzzy_score("zz", "Save scene").has_value());
}

TEST(FuzzyMatch, IgnoresCase) {
    EXPECT_EQ(fuzzy_score("UNDO", "Undo"), fuzzy_score("undo", "Undo"));
}

TEST(FuzzyMatch, NoMatchGivesAnEmptyRanking) { EXPECT_TRUE(fuzzy_rank("qqq", labels).empty()); }

TEST(FuzzyMatch, ConsecutiveMatchesRankFirst) { EXPECT_EQ(fuzzy_rank("st", labels).front(), 4U); }

TEST(FuzzyMatch, WordStartsBeatTheMiddleOfAWord) {
    EXPECT_GT(fuzzy_score("h", "Show hitboxes"), fuzzy_score("h", "Show"));
    EXPECT_EQ(fuzzy_rank("hit", labels).front(), 2U);
}

TEST(FuzzyMatch, InitialsFindAPhrase) { EXPECT_EQ(fuzzy_rank("stg", labels).front(), 3U); }

TEST(FuzzyMatch, EqualScoresKeepTheirOrder) {
    EXPECT_EQ(fuzzy_rank("s", labels), (std::vector<std::size_t>{0, 1, 2, 3, 4}));
}
