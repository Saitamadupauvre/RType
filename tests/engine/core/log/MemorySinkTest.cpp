#include <gtest/gtest.h>

#include <string>

#include "engine/core/log/MemorySink.hpp"

using engine::core::log::Level;
using engine::core::log::MemorySink;
using engine::core::log::Record;

TEST(MemorySink, StartsEmpty) {
    MemorySink sink;

    EXPECT_TRUE(sink.records().empty());
}

TEST(MemorySink, StoresRecordsInOrder) {
    MemorySink sink;

    sink.write(Record{.level = Level::Info, .message = "first"});
    sink.write(Record{.level = Level::Error, .message = "second"});

    const auto records = sink.records();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(records[0].level, Level::Info);
    EXPECT_EQ(records[0].message, "first");
    EXPECT_EQ(records[1].level, Level::Error);
    EXPECT_EQ(records[1].message, "second");
}

TEST(MemorySink, ClearRemovesRecords) {
    MemorySink sink;
    sink.write(Record{.level = Level::Warn, .message = "gone"});

    sink.clear();

    EXPECT_TRUE(sink.records().empty());
}

TEST(MemorySink, CopiesMessageBeyondTheCall) {
    MemorySink sink;
    {
        std::string temporary = "short-lived";
        sink.write(Record{.level = Level::Debug, .message = temporary});
        temporary.assign(temporary.size(), 'x');
    }

    const auto records = sink.records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].message, "short-lived");
}

TEST(MemorySink, AcceptsEmptyMessage) {
    MemorySink sink;

    sink.write(Record{.level = Level::Trace, .message = ""});

    const auto records = sink.records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_TRUE(records[0].message.empty());
}
