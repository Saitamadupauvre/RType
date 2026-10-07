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
using engine::core::SystemHandle;
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

TEST(SystemScheduler, RemovedSystemIsNoLongerCalled) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    const SystemHandle handle =
        scheduler.add("movement", 0, [&](EntityManager&, float) { ++calls; });

    EXPECT_TRUE(scheduler.remove(handle));
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 0);
}

TEST(SystemScheduler, RemoveKeepsTheOrderOfOtherSystems) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("first", 1, [&](EntityManager&, float) { calls.emplace_back("first"); });
    const SystemHandle middle = scheduler.add("middle", 2, [](EntityManager&, float) {});
    scheduler.add("last", 3, [&](EntityManager&, float) { calls.emplace_back("last"); });

    scheduler.remove(middle);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"first", "last"}));
}

TEST(SystemScheduler, RemoveReturnsFalseForUnknownHandle) {
    SystemScheduler scheduler;

    EXPECT_FALSE(scheduler.remove(SystemHandle{}));
    EXPECT_FALSE(scheduler.remove(SystemHandle{42}));
}

TEST(SystemScheduler, RemoveReturnsFalseWhenAlreadyRemoved) {
    SystemScheduler scheduler;
    const SystemHandle handle = scheduler.add("movement", 0, [](EntityManager&, float) {});
    scheduler.remove(handle);

    EXPECT_FALSE(scheduler.remove(handle));
}

TEST(SystemScheduler, RemoveFreesTheName) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    const SystemHandle handle = scheduler.add("movement", 0, [](EntityManager&, float) {});
    scheduler.remove(handle);

    EXPECT_NO_THROW(scheduler.add("movement", 0, [&](EntityManager&, float) { ++calls; }));
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 1);
}

TEST(SystemScheduler, RemoveDoesNotAffectTheHandleOfANewSystem) {
    SystemScheduler scheduler;
    const SystemHandle removed = scheduler.add("old", 0, [](EntityManager&, float) {});
    scheduler.remove(removed);

    const SystemHandle fresh = scheduler.add("new", 0, [](EntityManager&, float) {});

    EXPECT_NE(fresh.id, removed.id);
    EXPECT_FALSE(scheduler.remove(removed));
    EXPECT_TRUE(scheduler.remove(fresh));
}

TEST(SystemScheduler, RemovesADisabledSystem) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    const SystemHandle handle =
        scheduler.add("faulty", 0, [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.run(manager, 0.016F);

    EXPECT_TRUE(scheduler.remove(handle));
    EXPECT_NO_THROW(scheduler.add("faulty", 0, [](EntityManager&, float) {}));
}

TEST(SystemScheduler, SystemCanRemoveItselfDuringRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle handle;
    int calls = 0;
    handle = scheduler.add("once", 0, [&](EntityManager&, float) {
        ++calls;
        scheduler.remove(handle);
    });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 1);
}

TEST(SystemScheduler, SystemRemovedDuringRunBeforeItsTurnIsSkipped) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle victim;
    std::vector<std::string> calls;
    scheduler.add("killer", 1, [&](EntityManager&, float) {
        calls.emplace_back("killer");
        scheduler.remove(victim);
    });
    victim =
        scheduler.add("victim", 2, [&](EntityManager&, float) { calls.emplace_back("victim"); });
    scheduler.add("bystander", 3, [&](EntityManager&, float) { calls.emplace_back("bystander"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"killer", "bystander"}));
}

TEST(SystemScheduler, SystemRemovedDuringRunAfterItsTurnIsGoneNextRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle victim;
    int victim_calls = 0;
    victim = scheduler.add("victim", 1, [&](EntityManager&, float) { ++victim_calls; });
    scheduler.add("killer", 2, [&](EntityManager&, float) { scheduler.remove(victim); });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(victim_calls, 1);
}

TEST(SystemScheduler, RemovingTheSameSystemTwiceDuringRunReportsFalseTheSecondTime) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle victim;
    bool first = false;
    bool second = true;
    victim = scheduler.add("victim", 2, [](EntityManager&, float) {});
    scheduler.add("killer", 1, [&](EntityManager&, float) {
        first = scheduler.remove(victim);
        second = scheduler.remove(victim);
    });

    scheduler.run(manager, 0.016F);

    EXPECT_TRUE(first);
    EXPECT_FALSE(second);
}

TEST(SystemScheduler, AddDuringRunIsRejectedAndSchedulerKeepsWorking) {
    SystemScheduler scheduler;
    EntityManager manager;
    int rejected = 0;
    int calls = 0;
    scheduler.add("adder", 0, [&](EntityManager&, float) {
        ++calls;
        try {
            scheduler.add("late", 1, [](EntityManager&, float) {});
        } catch (const std::logic_error&) {
            ++rejected;
        }
    });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(rejected, 2);
    EXPECT_EQ(calls, 2);
    EXPECT_NO_THROW(scheduler.add("late", 1, [](EntityManager&, float) {}));
}
