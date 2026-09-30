#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

namespace logger = engine::core::log;
using engine::core::EntityManager;
using engine::core::SystemAlreadyRegistered;
using engine::core::SystemScheduler;

TEST(SystemScheduler, RunsSystemsByAscendingOrder) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("render", 30, [&](EntityManager&, float) { calls.emplace_back("render"); });
    scheduler.add("input", 10, [&](EntityManager&, float) { calls.emplace_back("input"); });
    scheduler.add("movement", 20, [&](EntityManager&, float) { calls.emplace_back("movement"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"input", "movement", "render"}));
}

TEST(SystemScheduler, BreaksTiesByRegistrationOrder) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("first", 5, [&](EntityManager&, float) { calls.emplace_back("first"); });
    scheduler.add("second", 5, [&](EntityManager&, float) { calls.emplace_back("second"); });
    scheduler.add("third", 5, [&](EntityManager&, float) { calls.emplace_back("third"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"first", "second", "third"}));
}

TEST(SystemScheduler, KeepsTheSameOrderOnEveryRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<int> calls;
    scheduler.add("b", 2, [&](EntityManager&, float) { calls.push_back(2); });
    scheduler.add("a", 1, [&](EntityManager&, float) { calls.push_back(1); });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<int>{1, 2, 1, 2}));
}

TEST(SystemScheduler, PassesEntityManagerAndTimeStepToSystems) {
    SystemScheduler scheduler;
    EntityManager manager;
    const EntityManager* received_manager = nullptr;
    float received_dt = 0.0F;
    scheduler.add("probe", 0, [&](EntityManager& reg, float dt) {
        received_manager = &reg;
        received_dt = dt;
    });

    scheduler.run(manager, 0.5F);

    EXPECT_EQ(received_manager, &manager);
    EXPECT_FLOAT_EQ(received_dt, 0.5F);
}

TEST(SystemScheduler, RunsWithoutSystems) {
    SystemScheduler scheduler;
    EntityManager manager;

    EXPECT_NO_THROW(scheduler.run(manager, 0.016F));
}

TEST(SystemScheduler, ReturnsDistinctHandles) {
    SystemScheduler scheduler;

    const auto first = scheduler.add("first", 0, [](EntityManager&, float) {});
    const auto second = scheduler.add("second", 0, [](EntityManager&, float) {});

    EXPECT_NE(first.id, second.id);
}

TEST(SystemScheduler, RejectsDuplicateNames) {
    SystemScheduler scheduler;
    scheduler.add("movement", 0, [](EntityManager&, float) {});

    EXPECT_THROW(scheduler.add("movement", 1, [](EntityManager&, float) {}),
                 SystemAlreadyRegistered);
}

TEST(SystemScheduler, KeepsExistingSystemAfterRejectingDuplicate) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    scheduler.add("movement", 0, [&](EntityManager&, float) { ++calls; });

    EXPECT_THROW(scheduler.add("movement", 1, [](EntityManager&, float) {}),
                 SystemAlreadyRegistered);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 1);
}

TEST(SystemScheduler, ThrowingSystemDoesNotStopOthers) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("before", 1, [&](EntityManager&, float) { calls.emplace_back("before"); });
    scheduler.add("faulty", 2, [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.add("after", 3, [&](EntityManager&, float) { calls.emplace_back("after"); });

    EXPECT_NO_THROW(scheduler.run(manager, 0.016F));

    EXPECT_EQ(calls, (std::vector<std::string>{"before", "after"}));
}

TEST(SystemScheduler, ThrowingSystemIsLoggedWithItsNameAndMessage) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    scheduler.add("faulty", 0, [](EntityManager&, float) { throw std::runtime_error("boom"); });

    scheduler.run(manager, 0.016F);

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].level, logger::Level::Error);
    EXPECT_NE(records[0].message.find("faulty"), std::string::npos);
    EXPECT_NE(records[0].message.find("boom"), std::string::npos);
}

TEST(SystemScheduler, ThrowingSystemIsDisabledOnNextRuns) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    int faulty_calls = 0;
    scheduler.add("faulty", 0, [&](EntityManager&, float) {
        ++faulty_calls;
        throw std::runtime_error("boom");
    });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(faulty_calls, 1);
    EXPECT_EQ(sink->records().size(), 1U);
}

TEST(SystemScheduler, NonStandardExceptionIsCaughtAndDisablesTheSystem) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    int faulty_calls = 0;
    int healthy_calls = 0;
    scheduler.add("faulty", 0, [&](EntityManager&, float) {
        ++faulty_calls;
        throw 7;
    });
    scheduler.add("healthy", 1, [&](EntityManager&, float) { ++healthy_calls; });

    EXPECT_NO_THROW(scheduler.run(manager, 0.016F));
    EXPECT_NO_THROW(scheduler.run(manager, 0.016F));

    EXPECT_EQ(faulty_calls, 1);
    EXPECT_EQ(healthy_calls, 2);
    EXPECT_EQ(sink->records().size(), 1U);
}

TEST(SystemScheduler, DisabledSystemKeepsItsNameReserved) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    scheduler.add("faulty", 0, [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.run(manager, 0.016F);

    EXPECT_THROW(scheduler.add("faulty", 0, [](EntityManager&, float) {}), SystemAlreadyRegistered);
}
