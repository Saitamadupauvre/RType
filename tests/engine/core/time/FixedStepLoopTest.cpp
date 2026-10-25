#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <stdexcept>

#include "engine/core/time/FixedStepLoop.hpp"
#include "engine/core/time/ManualClock.hpp"

using namespace std::chrono_literals;
using engine::core::time::Duration;
using engine::core::time::FixedStepConfig;
using engine::core::time::FixedStepLoop;
using engine::core::time::ManualClock;

namespace {

constexpr FixedStepConfig ten_ms_config{.step = 10ms, .max_steps_per_frame = 5};

std::size_t ticks_for(Duration total, Duration frame) {
    ManualClock clock;
    FixedStepLoop loop(clock, FixedStepConfig{.step = 10ms, .max_steps_per_frame = 1000});
    std::size_t ticks = 0;
    for (Duration elapsed = 0ns; elapsed + frame <= total; elapsed += frame) {
        clock.advance(frame);
        ticks += loop.run_frame([](Duration) {});
    }
    return ticks;
}

} // namespace

TEST(FixedStepLoop, DefaultsToSixtyHertz) {
    const ManualClock clock;
    const FixedStepLoop loop(clock);
    EXPECT_EQ(loop.step(), 1'000'000'000ns / 60);
}

TEST(FixedStepLoop, RunsNoStepBeforeAFullStepElapsed) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(9ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 0U);
}

TEST(FixedStepLoop, RunsOneStepPerWholeStepElapsed) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(30ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 3U);
}

TEST(FixedStepLoop, PassesTheFixedStepToUpdate) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(20ms);
    Duration seen = 0ns;
    loop.run_frame([&seen](Duration dt) { seen += dt; });
    EXPECT_EQ(seen, 20ms);
}

TEST(FixedStepLoop, CarriesTheRemainderToTheNextFrame) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(15ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 1U);
    clock.advance(5ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 1U);
}

TEST(FixedStepLoop, TickCountIsIndependentOfFrameRate) {
    const auto at_30_fps = ticks_for(1s, 33'333'333ns);
    const auto at_60_fps = ticks_for(1s, 16'666'666ns);
    const auto at_144_fps = ticks_for(1s, 6'944'444ns);
    const auto at_1000_fps = ticks_for(1s, 1ms);
    EXPECT_NEAR(static_cast<double>(at_30_fps), 100.0, 1.0);
    EXPECT_NEAR(static_cast<double>(at_60_fps), 100.0, 1.0);
    EXPECT_NEAR(static_cast<double>(at_144_fps), 100.0, 1.0);
    EXPECT_EQ(at_1000_fps, 100U);
}

TEST(FixedStepLoop, CapsCatchUpAfterALongPause) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(10s);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 5U);
}

TEST(FixedStepLoop, DropsTheTimeItCouldNotSimulate) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(10s);
    loop.run_frame([](Duration) {});
    EXPECT_EQ(loop.run_frame([](Duration) {}), 0U);
    clock.advance(10ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 1U);
}

TEST(FixedStepLoop, AlphaIsTheFractionOfAStepLeftOver) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    EXPECT_DOUBLE_EQ(loop.alpha(), 0.0);
    clock.advance(25ms);
    loop.run_frame([](Duration) {});
    EXPECT_DOUBLE_EQ(loop.alpha(), 0.5);
}

TEST(FixedStepLoop, AlphaIsBelowOneAfterACappedFrame) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(10s);
    loop.run_frame([](Duration) {});
    EXPECT_LT(loop.alpha(), 1.0);
}

TEST(FixedStepLoop, IgnoresTimeBeforeConstruction) {
    ManualClock clock;
    clock.advance(1h);
    FixedStepLoop loop(clock, ten_ms_config);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 0U);
}

TEST(FixedStepLoop, RejectsANonPositiveStep) {
    const ManualClock clock;
    EXPECT_THROW(FixedStepLoop(clock, FixedStepConfig{.step = 0ns, .max_steps_per_frame = 5}),
                 std::invalid_argument);
    EXPECT_THROW(FixedStepLoop(clock, FixedStepConfig{.step = -1ms, .max_steps_per_frame = 5}),
                 std::invalid_argument);
}

TEST(FixedStepLoop, RejectsAZeroStepCap) {
    const ManualClock clock;
    EXPECT_THROW(FixedStepLoop(clock, FixedStepConfig{.step = 10ms, .max_steps_per_frame = 0}),
                 std::invalid_argument);
}

TEST(FixedStepLoop, PropagatesUpdateExceptionsAndStaysUsable) {
    ManualClock clock;
    FixedStepLoop loop(clock, ten_ms_config);
    clock.advance(30ms);
    EXPECT_THROW(loop.run_frame([](Duration) { throw std::runtime_error("boom"); }),
                 std::runtime_error);
    clock.advance(10ms);
    EXPECT_EQ(loop.run_frame([](Duration) {}), 1U);
}
