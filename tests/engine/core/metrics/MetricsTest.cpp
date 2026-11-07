#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/time/ManualClock.hpp"

using namespace std::chrono_literals;
using engine::core::log::Level;
using engine::core::log::MemorySink;
using engine::core::log::ScopedSink;
using engine::core::metrics::MetricKind;
using engine::core::metrics::Metrics;
using engine::core::metrics::ScopedTimer;
using engine::core::time::Duration;
using engine::core::time::IClock;
using engine::core::time::ManualClock;

namespace {

class CountingClock final : public IClock {
public:
    [[nodiscard]] Duration now() const override {
        ++reads;
        return Duration::zero();
    }

    mutable int reads{0};
};

std::size_t warnings_in(const MemorySink& sink) {
    std::size_t count = 0;
    for (const auto& record : sink.records()) {
        if (record.level == Level::Warn) {
            ++count;
        }
    }
    return count;
}

} // namespace

TEST(Metrics, RejectsAZeroCapacity) {
    const ManualClock clock;
    EXPECT_THROW(Metrics(clock, 0), std::invalid_argument);
}

TEST(Metrics, UnknownNameHasNoSummaryAndNoHistory) {
    const ManualClock clock;
    const Metrics metrics(clock);

    EXPECT_FALSE(metrics.summary("tick").has_value());
    EXPECT_TRUE(metrics.history("tick").empty());
    EXPECT_TRUE(metrics.names().empty());
}

TEST(Metrics, AMetricHasNoSampleBeforeTheFirstFrameEnds) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.count("packets");

    const auto summary = metrics.summary("packets");
    ASSERT_TRUE(summary.has_value());
    EXPECT_EQ(summary->samples, 0U);
    EXPECT_TRUE(metrics.history("packets").empty());
}

TEST(Metrics, TimersSumTheirDurationsWithinAFrameInMilliseconds) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.record_time("lua.update", 2ms);
    metrics.record_time("lua.update", 500us);
    metrics.end_frame();

    const auto summary = metrics.summary("lua.update");
    ASSERT_TRUE(summary.has_value());
    EXPECT_EQ(summary->kind, MetricKind::timer);
    EXPECT_DOUBLE_EQ(summary->last, 2.5);
}

TEST(Metrics, TimersAndCountersReadZeroOnAFrameWithoutWrites) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.record_time("physics", 3ms);
    metrics.count("packets", 4);
    metrics.end_frame();
    metrics.end_frame();

    EXPECT_EQ(metrics.history("physics"), (std::vector<double>{3.0, 0.0}));
    EXPECT_EQ(metrics.history("packets"), (std::vector<double>{4.0, 0.0}));
}

TEST(Metrics, GaugesKeepTheirLastValueAcrossFrames) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.set_gauge("entities", 10);
    metrics.set_gauge("entities", 12);
    metrics.end_frame();
    metrics.end_frame();

    EXPECT_EQ(metrics.history("entities"), (std::vector<double>{12.0, 12.0}));
    EXPECT_EQ(metrics.summary("entities")->kind, MetricKind::gauge);
}

TEST(Metrics, SummaryGivesLastAverageAndMax) {
    const ManualClock clock;
    Metrics metrics(clock);

    for (const double value : {4.0, 10.0, 1.0}) {
        metrics.set_gauge("fps", value);
        metrics.end_frame();
    }

    const auto summary = metrics.summary("fps");
    ASSERT_TRUE(summary.has_value());
    EXPECT_DOUBLE_EQ(summary->last, 1.0);
    EXPECT_DOUBLE_EQ(summary->average, 5.0);
    EXPECT_DOUBLE_EQ(summary->max, 10.0);
    EXPECT_EQ(summary->samples, 3U);
}

TEST(Metrics, HistoryKeepsOnlyTheLatestFramesOnceFull) {
    const ManualClock clock;
    Metrics metrics(clock, 3);

    for (int frame = 1; frame <= 5; ++frame) {
        metrics.count("frames", frame);
        metrics.end_frame();
    }

    EXPECT_EQ(metrics.history("frames"), (std::vector<double>{3.0, 4.0, 5.0}));
    const auto summary = metrics.summary("frames");
    EXPECT_EQ(summary->samples, 3U);
    EXPECT_DOUBLE_EQ(summary->average, 4.0);
    EXPECT_DOUBLE_EQ(summary->max, 5.0);
}

TEST(Metrics, AMetricWrittenLaterOnlyHasTheFramesSinceItsFirstWrite) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.count("early");
    metrics.end_frame();
    metrics.count("late", 2);
    metrics.end_frame();

    EXPECT_EQ(metrics.history("early").size(), 2U);
    EXPECT_EQ(metrics.history("late"), (std::vector<double>{2.0}));
}

TEST(Metrics, NamesAreSorted) {
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.set_gauge("tick", 1);
    metrics.count("entities");
    metrics.record_time("fps", 1ms);

    EXPECT_EQ(metrics.names(), (std::vector<std::string>{"entities", "fps", "tick"}));
}

TEST(Metrics, AWriteOfAnotherKindIsDroppedAndLoggedOnce) {
    auto sink = std::make_shared<MemorySink>();
    const ScopedSink scoped(sink, Level::Trace);
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.record_time("tick", 1ms);
    metrics.count("tick", 5);
    metrics.set_gauge("tick", 7);
    metrics.end_frame();

    EXPECT_EQ(metrics.summary("tick")->kind, MetricKind::timer);
    EXPECT_EQ(metrics.history("tick"), (std::vector<double>{1.0}));
    EXPECT_EQ(warnings_in(*sink), 1U);
}

TEST(Metrics, EmptyNamesAndNonFiniteValuesAreDropped) {
    auto sink = std::make_shared<MemorySink>();
    const ScopedSink scoped(sink, Level::Trace);
    const ManualClock clock;
    Metrics metrics(clock);

    metrics.count("");
    metrics.set_gauge("fps", std::numeric_limits<double>::quiet_NaN());
    metrics.set_gauge("fps", std::numeric_limits<double>::infinity());
    metrics.set_gauge("fps", 60);
    metrics.end_frame();

    EXPECT_EQ(metrics.names(), (std::vector<std::string>{"fps"}));
    EXPECT_EQ(metrics.history("fps"), (std::vector<double>{60.0}));
    EXPECT_EQ(warnings_in(*sink), 2U);
}

TEST(Metrics, DisabledMetricsRecordNothingAndKeepTheirHistory) {
    const ManualClock clock;
    Metrics metrics(clock);
    metrics.set_gauge("fps", 30);
    metrics.end_frame();

    metrics.set_enabled(false);
    metrics.set_gauge("fps", 60);
    metrics.count("packets");
    metrics.record_time("tick", 1ms);
    metrics.end_frame();

    EXPECT_FALSE(metrics.enabled());
    EXPECT_EQ(metrics.names(), (std::vector<std::string>{"fps"}));
    EXPECT_EQ(metrics.history("fps"), (std::vector<double>{30.0}));
}

TEST(Metrics, CollectionResumesOnceEnabledAgain) {
    const ManualClock clock;
    Metrics metrics(clock);
    metrics.set_enabled(false);
    metrics.set_enabled(true);

    metrics.count("packets");
    metrics.end_frame();

    EXPECT_EQ(metrics.history("packets"), (std::vector<double>{1.0}));
}

TEST(ScopedTimer, RecordsTheTimeSpentInItsScope) {
    ManualClock clock;
    Metrics metrics(clock);

    {
        const ScopedTimer timer(metrics, "lua.update");
        clock.advance(4ms);
    }
    {
        const ScopedTimer timer(metrics, "lua.update");
        clock.advance(1ms);
    }
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("lua.update")->last, 5.0);
}

TEST(ScopedTimer, NestedTimersMeasureTheirOwnScope) {
    ManualClock clock;
    Metrics metrics(clock);

    {
        const ScopedTimer outer(metrics, "tick");
        clock.advance(1ms);
        {
            const ScopedTimer inner(metrics, "physics");
            clock.advance(2ms);
        }
    }
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("tick")->last, 3.0);
    EXPECT_DOUBLE_EQ(metrics.summary("physics")->last, 2.0);
}

TEST(ScopedTimer, DoesNotReadTheClockWhileMetricsAreDisabled) {
    const CountingClock clock;
    Metrics metrics(clock);
    metrics.set_enabled(false);

    {
        const ScopedTimer timer(metrics, "tick");
    }

    EXPECT_EQ(clock.reads, 0);
    EXPECT_TRUE(metrics.names().empty());
}

TEST(ScopedTimer, RecordsNothingIfMetricsWereDisabledWhenItStarted) {
    ManualClock clock;
    Metrics metrics(clock);
    metrics.set_enabled(false);

    {
        const ScopedTimer timer(metrics, "tick");
        metrics.set_enabled(true);
        clock.advance(1ms);
    }
    metrics.end_frame();

    EXPECT_TRUE(metrics.names().empty());
}

TEST(Metrics, ConcurrentWritersLoseNoUpdate) {
    const ManualClock clock;
    Metrics metrics(clock);
    constexpr int writers = 4;
    constexpr int writes_per_writer = 1000;

    std::vector<std::thread> threads;
    threads.reserve(writers);
    for (int writer = 0; writer < writers; ++writer) {
        threads.emplace_back([&metrics, writer] {
            const std::string gauge = "writer." + std::to_string(writer);
            for (int write = 0; write < writes_per_writer; ++write) {
                metrics.count("packets");
                metrics.record_time("tick", 1ms);
                metrics.set_gauge(gauge, write);
                static_cast<void>(metrics.summary("packets"));
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    metrics.end_frame();

    EXPECT_DOUBLE_EQ(metrics.summary("packets")->last, writers * writes_per_writer);
    EXPECT_DOUBLE_EQ(metrics.summary("tick")->last, writers * writes_per_writer);
    for (int writer = 0; writer < writers; ++writer) {
        EXPECT_DOUBLE_EQ(metrics.summary("writer." + std::to_string(writer))->last,
                         writes_per_writer - 1);
    }
}

TEST(Metrics, ReadersSeeCompleteFramesWhileAWriterEndsFrames) {
    const ManualClock clock;
    Metrics metrics(clock, 8);
    constexpr int frames = 500;

    std::thread writer([&metrics] {
        for (int frame = 0; frame < frames; ++frame) {
            metrics.count("a", 1);
            metrics.count("a", 1);
            metrics.end_frame();
        }
    });
    bool every_sample_complete = true;
    for (int read = 0; read < frames; ++read) {
        for (const double sample : metrics.history("a")) {
            every_sample_complete = every_sample_complete && sample == 2.0;
        }
    }
    writer.join();

    EXPECT_TRUE(every_sample_complete);
    EXPECT_EQ(metrics.history("a").size(), 8U);
}
