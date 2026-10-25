#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

#include "engine/core/time/ManualClock.hpp"
#include "engine/core/time/SteadyClock.hpp"

using namespace std::chrono_literals;
using engine::core::time::ManualClock;
using engine::core::time::SteadyClock;

TEST(ManualClock, StartsAtZero) {
    const ManualClock clock;
    EXPECT_EQ(clock.now(), 0ns);
}

TEST(ManualClock, AdvancesByTheGivenAmount) {
    ManualClock clock;
    clock.advance(5ms);
    clock.advance(7ms);
    EXPECT_EQ(clock.now(), 12ms);
}

TEST(ManualClock, RejectsNegativeAdvance) {
    ManualClock clock;
    clock.advance(5ms);
    EXPECT_THROW(clock.advance(-1ns), std::invalid_argument);
    EXPECT_EQ(clock.now(), 5ms);
}

TEST(SteadyClock, NeverGoesBackwards) {
    const SteadyClock clock;
    const auto first = clock.now();
    EXPECT_GE(clock.now(), first);
    EXPECT_GE(first, 0ns);
}
