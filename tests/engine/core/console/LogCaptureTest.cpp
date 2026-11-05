#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "engine/core/console/LogCapture.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

namespace log = engine::core::log;
using engine::core::console::LogCapture;

namespace {

class ThrowingSink final : public log::Sink {
public:
    void write(const log::Record&) override { throw std::runtime_error("sink failed"); }
};

} // namespace

TEST(LogCapture, ForwardsAndKeepsEveryRecord) {
    const auto next = std::make_shared<log::MemorySink>();
    const auto capture = std::make_shared<LogCapture>(next);
    const log::ScopedSink scoped(capture, log::Level::Info);

    log::info("hello {}", 1);
    log::error("failed");

    const auto taken = capture->take();
    ASSERT_EQ(taken.size(), 2U);
    EXPECT_EQ(taken[0].message, "hello 1");
    EXPECT_EQ(taken[1].level, log::Level::Error);
    EXPECT_EQ(next->records().size(), 2U);
    EXPECT_TRUE(capture->take().empty());
}

TEST(LogCapture, DropsTheOldestRecordsBeyondItsCapacity) {
    LogCapture capture(nullptr, 2);
    for (const auto* message : {"a", "b", "c"}) {
        capture.write({.level = log::Level::Info, .message = message});
    }
    const auto taken = capture.take();
    ASSERT_EQ(taken.size(), 2U);
    EXPECT_EQ(taken[0].message, "b");
    EXPECT_EQ(taken[1].message, "c");
}

TEST(LogCapture, KeepsRecordsWhenTheNextSinkThrows) {
    LogCapture capture(std::make_shared<ThrowingSink>());
    EXPECT_NO_THROW(capture.write({.level = log::Level::Warn, .message = "still kept"}));
    ASSERT_EQ(capture.take().size(), 1U);
}

TEST(LogCapture, AcceptsRecordsFromSeveralThreads) {
    LogCapture capture(nullptr, 1000);
    std::vector<std::jthread> writers;
    for (int writer = 0; writer < 4; ++writer) {
        writers.emplace_back([&capture] {
            for (int message = 0; message < 100; ++message) {
                capture.write({.level = log::Level::Info, .message = "line"});
            }
        });
    }
    writers.clear();
    EXPECT_EQ(capture.take().size(), 400U);
}
