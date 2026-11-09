#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "engine/core/console/LogCapture.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

namespace logging = engine::core::log;
using engine::core::console::LogCapture;

namespace {

class ThrowingSink final : public logging::Sink {
public:
    void write(const logging::Record&) override { throw std::runtime_error("sink failed"); }
};

} // namespace

TEST(LogCapture, ForwardsAndKeepsEveryRecord) {
    const auto next = std::make_shared<logging::MemorySink>();
    const auto capture = std::make_shared<LogCapture>(next);
    const logging::ScopedSink scoped(capture, logging::Level::Info);

    logging::info("hello {}", 1);
    logging::error("failed");

    const auto taken = capture->take();
    ASSERT_EQ(taken.size(), 2U);
    EXPECT_EQ(taken[0].message, "hello 1");
    EXPECT_EQ(taken[1].level, logging::Level::Error);
    EXPECT_EQ(next->records().size(), 2U);
    EXPECT_TRUE(capture->take().empty());
}

TEST(LogCapture, DropsTheOldestRecordsBeyondItsCapacity) {
    LogCapture capture(nullptr, 2);
    for (const auto* message : {"a", "b", "c"}) {
        capture.write({.level = logging::Level::Info, .message = message});
    }
    const auto taken = capture.take();
    ASSERT_EQ(taken.size(), 2U);
    EXPECT_EQ(taken[0].message, "b");
    EXPECT_EQ(taken[1].message, "c");
}

TEST(LogCapture, KeepsRecordsWhenTheNextSinkThrows) {
    LogCapture capture(std::make_shared<ThrowingSink>());
    EXPECT_NO_THROW(capture.write({.level = logging::Level::Warn, .message = "still kept"}));
    ASSERT_EQ(capture.take().size(), 1U);
}

TEST(LogCapture, AcceptsRecordsFromSeveralThreads) {
    LogCapture capture(nullptr, 1000);
    std::vector<std::jthread> writers;
    writers.reserve(4);
    for (int writer = 0; writer < 4; ++writer) {
        writers.emplace_back([&capture] {
            for (int message = 0; message < 100; ++message) {
                capture.write({.level = logging::Level::Info, .message = "line"});
            }
        });
    }
    writers.clear();
    EXPECT_EQ(capture.take().size(), 400U);
}
