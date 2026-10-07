#include <gtest/gtest.h>

#include <atomic>
#include <format>
#include <latch>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

namespace logger = engine::core::log;
using logger::Level;
using logger::MemorySink;
using logger::Record;
using logger::ScopedSink;

namespace {

class ThrowingSink final : public logger::Sink {
public:
    void write(const Record&) override { throw std::runtime_error("sink failure"); }
};

struct ThrowingFormat {};

} // namespace

template <> struct std::formatter<ThrowingFormat> {
    constexpr auto parse(std::format_parse_context& context) { return context.begin(); }
    auto format(const ThrowingFormat&, std::format_context&) const
        -> std::format_context::iterator {
        throw std::runtime_error("format failure");
    }
};

TEST(Log, ExposesConfiguredLevel) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Warn);

    EXPECT_EQ(logger::level(), Level::Warn);
    logger::set_level(Level::Debug);
    EXPECT_EQ(logger::level(), Level::Debug);
}

TEST(Log, ScopedSinkRestoresPreviousState) {
    auto outer = std::make_shared<MemorySink>();
    ScopedSink outer_scope(outer, Level::Error);
    {
        auto inner = std::make_shared<MemorySink>();
        ScopedSink inner_scope(inner, Level::Trace);
        EXPECT_EQ(logger::current_sink(), inner);
    }

    EXPECT_EQ(logger::current_sink(), outer);
    EXPECT_EQ(logger::level(), Level::Error);
}

TEST(Log, FiltersBelowLevel) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Warn);

    logger::trace("dropped");
    logger::debug("dropped");
    logger::info("dropped");
    logger::warn("kept");
    logger::error("kept");

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(records[0].level, Level::Warn);
    EXPECT_EQ(records[1].level, Level::Error);
}

TEST(Log, KeepsEveryLevelAtTrace) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    logger::trace("t");
    logger::debug("d");
    logger::info("i");
    logger::warn("w");
    logger::error("e");

    EXPECT_EQ(sink->records().size(), 5U);
}

TEST(Log, OffLevelDropsEverything) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Off);

    logger::error("silenced");

    EXPECT_TRUE(sink->records().empty());
}

TEST(Log, CapturesMessagesInOrder) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    logger::info("one");
    logger::info("two");
    logger::info("three");

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 3U);
    EXPECT_EQ(records[0].message, "one");
    EXPECT_EQ(records[1].message, "two");
    EXPECT_EQ(records[2].message, "three");
}

TEST(Log, FormatsArguments) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    logger::info("{} has {} hp", "ship", 3);

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].message, "ship has 3 hp");
}

TEST(Log, WriteBypassesFormatting) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    logger::write(Level::Info, "{} stays raw");

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].message, "{} stays raw");
}

TEST(Log, SurvivesThrowingSink) {
    ScopedSink scoped(std::make_shared<ThrowingSink>(), Level::Trace);

    EXPECT_NO_THROW(logger::error("boom"));
}

TEST(Log, SurvivesThrowingFormatter) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    EXPECT_NO_THROW(logger::error("value {}", ThrowingFormat{}));

    logger::info("still alive");
    const auto records = sink->records();
    ASSERT_FALSE(records.empty());
    EXPECT_EQ(records.back().message, "still alive");
}

TEST(Log, NullSinkRestoresDefault) {
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);

    logger::set_sink(nullptr);

    EXPECT_NE(logger::current_sink(), nullptr);
    EXPECT_NE(logger::current_sink(), sink);
    EXPECT_NO_THROW(logger::info("goes to the default sink"));
    EXPECT_TRUE(sink->records().empty());
}

TEST(Log, ConcurrentWritersLoseNothing) {
    constexpr int thread_count = 8;
    constexpr int messages_per_thread = 500;
    auto sink = std::make_shared<MemorySink>();
    ScopedSink scoped(sink, Level::Trace);
    std::latch start(thread_count);

    std::vector<std::jthread> threads;
    threads.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&start, t] {
            start.arrive_and_wait();
            for (int i = 0; i < messages_per_thread; ++i) {
                logger::info("thread {} message {}", t, i);
            }
        });
    }
    threads.clear();

    EXPECT_EQ(sink->records().size(), static_cast<std::size_t>(thread_count * messages_per_thread));
}

TEST(Log, SinkSwapDuringLoggingIsSafe) {
    constexpr int thread_count = 4;
    constexpr int messages_per_thread = 500;
    auto first = std::make_shared<MemorySink>();
    auto second = std::make_shared<MemorySink>();
    ScopedSink scoped(first, Level::Trace);
    std::atomic<int> finished{0};
    std::latch start(thread_count + 1);

    std::vector<std::jthread> writers;
    writers.reserve(thread_count);
    for (int t = 0; t < thread_count; ++t) {
        writers.emplace_back([&start, &finished] {
            start.arrive_and_wait();
            for (int i = 0; i < messages_per_thread; ++i) {
                logger::info("message {}", i);
            }
            finished.fetch_add(1);
        });
    }

    start.arrive_and_wait();
    bool use_second = true;
    while (finished.load() < thread_count) {
        logger::set_sink(use_second ? second : first);
        use_second = !use_second;
        std::this_thread::yield();
    }
    writers.clear();

    EXPECT_EQ(first->records().size() + second->records().size(),
              static_cast<std::size_t>(thread_count * messages_per_thread));
}
