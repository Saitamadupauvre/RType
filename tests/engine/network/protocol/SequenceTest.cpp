#include <cstdint>

#include <gtest/gtest.h>

#include "protocol/Sequence.hpp"

using engine::network::is_newer_sequence;

TEST(Sequence, LargerSequenceIsNewer) {
    EXPECT_TRUE(is_newer_sequence(2, 1));
    EXPECT_FALSE(is_newer_sequence(1, 2));
}

TEST(Sequence, EqualSequenceIsNotNewer) { EXPECT_FALSE(is_newer_sequence(7, 7)); }

TEST(Sequence, WrapsAroundFromMaximumToZero) {
    EXPECT_TRUE(is_newer_sequence(0, 0xFFFF'FFFF));
    EXPECT_TRUE(is_newer_sequence(5, 0xFFFF'FFF0));
    EXPECT_FALSE(is_newer_sequence(0xFFFF'FFFF, 0));
}

TEST(Sequence, HalfRangeAheadIsOlder) {
    EXPECT_TRUE(is_newer_sequence(0x7FFF'FFFF, 0));
    EXPECT_FALSE(is_newer_sequence(0x8000'0000, 0));
}
