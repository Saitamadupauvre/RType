#include <gtest/gtest.h>

#include <cstddef>
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
using engine::core::SystemPhase;

TEST(SystemScheduler, RunsSystemsByPhaseOrder) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("render", SystemPhase::Replicate,
                  [&](EntityManager&, float) { calls.emplace_back("render"); });
    scheduler.add("input", SystemPhase::Input,
                  [&](EntityManager&, float) { calls.emplace_back("input"); });
    scheduler.add("movement", SystemPhase::Movement,
                  [&](EntityManager&, float) { calls.emplace_back("movement"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"input", "movement", "render"}));
}

TEST(SystemScheduler, RunsEveryPhaseInTheDocumentedOrderRegardlessOfRegistrationOrder) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<SystemPhase> calls;
    const std::vector<SystemPhase> registration_order{
        SystemPhase::Replicate, SystemPhase::Destroy, SystemPhase::Collision,
        SystemPhase::Movement,  SystemPhase::Callbacks, SystemPhase::Script, SystemPhase::Input};

    for (std::size_t index = 0; index < registration_order.size(); ++index) {
        const SystemPhase phase = registration_order[index];
        scheduler.add(std::to_string(index), phase,
                      [&, phase](EntityManager&, float) { calls.push_back(phase); });
    }

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<SystemPhase>{SystemPhase::Input, SystemPhase::Script,
                                                SystemPhase::Callbacks, SystemPhase::Movement,
                                                SystemPhase::Collision, SystemPhase::Destroy,
                                                SystemPhase::Replicate}));
}

TEST(SystemScheduler, BreaksTiesByRegistrationOrder) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("first", SystemPhase::Collision,
                  [&](EntityManager&, float) { calls.emplace_back("first"); });
    scheduler.add("second", SystemPhase::Collision,
                  [&](EntityManager&, float) { calls.emplace_back("second"); });
    scheduler.add("third", SystemPhase::Collision,
                  [&](EntityManager&, float) { calls.emplace_back("third"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"first", "second", "third"}));
}

TEST(SystemScheduler, KeepsTheSameOrderOnEveryRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<int> calls;
    scheduler.add("b", SystemPhase::Callbacks, [&](EntityManager&, float) { calls.push_back(2); });
    scheduler.add("a", SystemPhase::Script, [&](EntityManager&, float) { calls.push_back(1); });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<int>{1, 2, 1, 2}));
}

TEST(SystemScheduler, PassesEntityManagerAndTimeStepToSystems) {
    SystemScheduler scheduler;
    EntityManager manager;
    const EntityManager* received_manager = nullptr;
    float received_dt = 0.0F;
    scheduler.add("probe", SystemPhase::Input, [&](EntityManager& reg, float dt) {
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

    const auto first = scheduler.add("first", SystemPhase::Input, [](EntityManager&, float) {});
    const auto second = scheduler.add("second", SystemPhase::Input, [](EntityManager&, float) {});

    EXPECT_NE(first.id, second.id);
}

TEST(SystemScheduler, RejectsDuplicateNames) {
    SystemScheduler scheduler;
    scheduler.add("movement", SystemPhase::Input, [](EntityManager&, float) {});

    EXPECT_THROW(scheduler.add("movement", SystemPhase::Script, [](EntityManager&, float) {}),
                 SystemAlreadyRegistered);
}

TEST(SystemScheduler, KeepsExistingSystemAfterRejectingDuplicate) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    scheduler.add("movement", SystemPhase::Input, [&](EntityManager&, float) { ++calls; });

    EXPECT_THROW(scheduler.add("movement", SystemPhase::Script, [](EntityManager&, float) {}),
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
    scheduler.add("before", SystemPhase::Script,
                  [&](EntityManager&, float) { calls.emplace_back("before"); });
    scheduler.add("faulty", SystemPhase::Callbacks,
                  [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.add("after", SystemPhase::Movement,
                  [&](EntityManager&, float) { calls.emplace_back("after"); });

    EXPECT_NO_THROW(scheduler.run(manager, 0.016F));

    EXPECT_EQ(calls, (std::vector<std::string>{"before", "after"}));
}

TEST(SystemScheduler, ThrowingSystemIsLoggedWithItsNameAndMessage) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    SystemScheduler scheduler;
    EntityManager manager;
    scheduler.add("faulty", SystemPhase::Input,
                  [](EntityManager&, float) { throw std::runtime_error("boom"); });

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
    scheduler.add("faulty", SystemPhase::Input, [&](EntityManager&, float) {
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
    scheduler.add("faulty", SystemPhase::Input, [&](EntityManager&, float) {
        ++faulty_calls;
        throw 7;
    });
    scheduler.add("healthy", SystemPhase::Script, [&](EntityManager&, float) { ++healthy_calls; });

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
    scheduler.add("faulty", SystemPhase::Input,
                  [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.run(manager, 0.016F);

    EXPECT_THROW(scheduler.add("faulty", SystemPhase::Input, [](EntityManager&, float) {}),
                 SystemAlreadyRegistered);
}

TEST(SystemScheduler, RemovedSystemIsNoLongerCalled) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    const SystemHandle handle =
        scheduler.add("movement", SystemPhase::Input, [&](EntityManager&, float) { ++calls; });

    EXPECT_TRUE(scheduler.remove(handle));
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 0);
}

TEST(SystemScheduler, RemoveKeepsTheOrderOfOtherSystems) {
    SystemScheduler scheduler;
    EntityManager manager;
    std::vector<std::string> calls;
    scheduler.add("first", SystemPhase::Script,
                  [&](EntityManager&, float) { calls.emplace_back("first"); });
    const SystemHandle middle =
        scheduler.add("middle", SystemPhase::Callbacks, [](EntityManager&, float) {});
    scheduler.add("last", SystemPhase::Movement,
                  [&](EntityManager&, float) { calls.emplace_back("last"); });

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
    const SystemHandle handle =
        scheduler.add("movement", SystemPhase::Input, [](EntityManager&, float) {});
    scheduler.remove(handle);

    EXPECT_FALSE(scheduler.remove(handle));
}

TEST(SystemScheduler, RemoveFreesTheName) {
    SystemScheduler scheduler;
    EntityManager manager;
    int calls = 0;
    const SystemHandle handle =
        scheduler.add("movement", SystemPhase::Input, [](EntityManager&, float) {});
    scheduler.remove(handle);

    EXPECT_NO_THROW(
        scheduler.add("movement", SystemPhase::Input, [&](EntityManager&, float) { ++calls; }));
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, 1);
}

TEST(SystemScheduler, RemoveDoesNotAffectTheHandleOfANewSystem) {
    SystemScheduler scheduler;
    const SystemHandle removed =
        scheduler.add("old", SystemPhase::Input, [](EntityManager&, float) {});
    scheduler.remove(removed);

    const SystemHandle fresh =
        scheduler.add("new", SystemPhase::Input, [](EntityManager&, float) {});

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
        scheduler.add("faulty", SystemPhase::Input,
                      [](EntityManager&, float) { throw std::runtime_error("boom"); });
    scheduler.run(manager, 0.016F);

    EXPECT_TRUE(scheduler.remove(handle));
    EXPECT_NO_THROW(scheduler.add("faulty", SystemPhase::Input, [](EntityManager&, float) {}));
}

TEST(SystemScheduler, SystemCanRemoveItselfDuringRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle handle;
    int calls = 0;
    handle = scheduler.add("once", SystemPhase::Input, [&](EntityManager&, float) {
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
    scheduler.add("killer", SystemPhase::Script, [&](EntityManager&, float) {
        calls.emplace_back("killer");
        scheduler.remove(victim);
    });
    victim =
        scheduler.add("victim", SystemPhase::Callbacks,
                      [&](EntityManager&, float) { calls.emplace_back("victim"); });
    scheduler.add("bystander", SystemPhase::Movement,
                  [&](EntityManager&, float) { calls.emplace_back("bystander"); });

    scheduler.run(manager, 0.016F);

    EXPECT_EQ(calls, (std::vector<std::string>{"killer", "bystander"}));
}

TEST(SystemScheduler, SystemRemovedDuringRunAfterItsTurnIsGoneNextRun) {
    SystemScheduler scheduler;
    EntityManager manager;
    SystemHandle victim;
    int victim_calls = 0;
    victim = scheduler.add("victim", SystemPhase::Script,
                           [&](EntityManager&, float) { ++victim_calls; });
    scheduler.add("killer", SystemPhase::Callbacks,
                  [&](EntityManager&, float) { scheduler.remove(victim); });

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
    victim = scheduler.add("victim", SystemPhase::Callbacks, [](EntityManager&, float) {});
    scheduler.add("killer", SystemPhase::Script, [&](EntityManager&, float) {
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
    scheduler.add("adder", SystemPhase::Input, [&](EntityManager&, float) {
        ++calls;
        try {
            scheduler.add("late", SystemPhase::Script, [](EntityManager&, float) {});
        } catch (const std::logic_error&) {
            ++rejected;
        }
    });

    scheduler.run(manager, 0.016F);
    scheduler.run(manager, 0.016F);

    EXPECT_EQ(rejected, 2);
    EXPECT_EQ(calls, 2);
    EXPECT_NO_THROW(scheduler.add("late", SystemPhase::Script, [](EntityManager&, float) {}));
}
