#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

namespace event = engine::core::event;
namespace logger = engine::core::log;
using event::EventBus;
using event::Subscription;

namespace {

struct Damage {
    int amount;
};

struct Heal {
    int amount;
};

struct MoveOnly {
    std::unique_ptr<int> value;
};

} // namespace

TEST(EventBus, DeliversOnlyToHandlersOfTheEventType) {
    EventBus bus;
    std::vector<int> damage;
    std::vector<int> heal;
    bus.subscribe<Damage>([&](const Damage& e) { damage.push_back(e.amount); });
    bus.subscribe<Heal>([&](const Heal& e) { heal.push_back(e.amount); });

    bus.publish(Damage{3});

    EXPECT_EQ(damage, std::vector<int>{3});
    EXPECT_TRUE(heal.empty());
}

TEST(EventBus, CallsHandlersInSubscriptionOrder) {
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<Damage>([&](const Damage&) { order.push_back(1); });
    bus.subscribe<Damage>([&](const Damage&) { order.push_back(2); });
    bus.subscribe<Damage>([&](const Damage&) { order.push_back(3); });

    bus.publish(Damage{0});

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(EventBus, PublishWithoutSubscribersDoesNothing) {
    EventBus bus;

    EXPECT_NO_THROW(bus.publish(Damage{1}));
    EXPECT_NO_THROW(bus.dispatch());
}

TEST(EventBus, EnqueuedEventIsNotDeliveredBeforeDispatch) {
    EventBus bus;
    int calls = 0;
    bus.subscribe<Damage>([&](const Damage&) { ++calls; });

    bus.enqueue(Damage{1});

    EXPECT_EQ(calls, 0);
    bus.dispatch();
    EXPECT_EQ(calls, 1);
}

TEST(EventBus, DispatchDeliversQueuedEventsOldestFirst) {
    EventBus bus;
    std::vector<int> received;
    bus.subscribe<Damage>([&](const Damage& e) { received.push_back(e.amount); });

    bus.enqueue(Damage{1});
    bus.enqueue(Damage{2});
    bus.enqueue(Damage{3});
    bus.dispatch();

    EXPECT_EQ(received, (std::vector<int>{1, 2, 3}));
}

TEST(EventBus, DispatchEmptiesTheQueue) {
    EventBus bus;
    int calls = 0;
    bus.subscribe<Damage>([&](const Damage&) { ++calls; });

    bus.enqueue(Damage{1});
    bus.dispatch();
    bus.dispatch();

    EXPECT_EQ(calls, 1);
}

TEST(EventBus, DispatchKeepsEventTypesSeparate) {
    EventBus bus;
    std::vector<std::string> received;
    bus.subscribe<Damage>([&](const Damage&) { received.emplace_back("damage"); });
    bus.subscribe<Heal>([&](const Heal&) { received.emplace_back("heal"); });

    bus.enqueue(Heal{1});
    bus.enqueue(Damage{1});
    bus.dispatch();

    EXPECT_EQ(received, (std::vector<std::string>{"heal", "damage"}));
}

TEST(EventBus, EventEnqueuedByHandlerWaitsForNextDispatch) {
    EventBus bus;
    int damage_calls = 0;
    bus.subscribe<Heal>([&](const Heal&) { bus.enqueue(Damage{1}); });
    bus.subscribe<Damage>([&](const Damage&) { ++damage_calls; });

    bus.enqueue(Heal{1});
    bus.dispatch();
    EXPECT_EQ(damage_calls, 0);

    bus.dispatch();
    EXPECT_EQ(damage_calls, 1);
}

TEST(EventBus, EnqueueAcceptsMoveOnlyEvents) {
    EventBus bus;
    int received = 0;
    bus.subscribe<MoveOnly>([&](const MoveOnly& e) { received = *e.value; });

    bus.enqueue(MoveOnly{std::make_unique<int>(42)});
    bus.dispatch();

    EXPECT_EQ(received, 42);
}

TEST(EventBus, UnsubscribeStopsDelivery) {
    EventBus bus;
    int calls = 0;
    const Subscription subscription = bus.subscribe<Damage>([&](const Damage&) { ++calls; });

    bus.publish(Damage{1});
    bus.unsubscribe(subscription);
    bus.publish(Damage{1});

    EXPECT_EQ(calls, 1);
}

TEST(EventBus, UnsubscribeOnlyRemovesTheGivenHandler) {
    EventBus bus;
    int first = 0;
    int second = 0;
    const Subscription removed = bus.subscribe<Damage>([&](const Damage&) { ++first; });
    bus.subscribe<Damage>([&](const Damage&) { ++second; });

    bus.unsubscribe(removed);
    bus.publish(Damage{1});

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(EventBus, UnsubscribeIgnoresUnknownAndRepeatedHandles) {
    EventBus bus;
    const Subscription subscription = bus.subscribe<Damage>([](const Damage&) {});

    EXPECT_NO_THROW(bus.unsubscribe(Subscription{}));
    EXPECT_NO_THROW(bus.unsubscribe(Subscription{.type = 999, .id = 999}));
    bus.unsubscribe(subscription);
    EXPECT_NO_THROW(bus.unsubscribe(subscription));
}

TEST(EventBus, HandlerCanUnsubscribeItselfDuringPublish) {
    EventBus bus;
    int calls = 0;
    Subscription self;
    self = bus.subscribe<Damage>([&](const Damage&) {
        ++calls;
        bus.unsubscribe(self);
    });

    bus.publish(Damage{1});
    bus.publish(Damage{1});

    EXPECT_EQ(calls, 1);
}

TEST(EventBus, HandlerUnsubscribedByEarlierHandlerIsNotCalled) {
    EventBus bus;
    int later_calls = 0;
    Subscription later;
    bus.subscribe<Damage>([&](const Damage&) { bus.unsubscribe(later); });
    later = bus.subscribe<Damage>([&](const Damage&) { ++later_calls; });

    bus.publish(Damage{1});

    EXPECT_EQ(later_calls, 0);
}

TEST(EventBus, HandlerCanUnsubscribeDuringDispatch) {
    EventBus bus;
    int calls = 0;
    Subscription self;
    self = bus.subscribe<Damage>([&](const Damage&) {
        ++calls;
        bus.unsubscribe(self);
    });

    bus.enqueue(Damage{1});
    bus.enqueue(Damage{2});
    bus.dispatch();

    EXPECT_EQ(calls, 1);
}

TEST(EventBus, HandlerSubscribedDuringPublishMissesTheCurrentEvent) {
    EventBus bus;
    int late_calls = 0;
    bool subscribed = false;
    bus.subscribe<Damage>([&](const Damage&) {
        if (!subscribed) {
            subscribed = true;
            bus.subscribe<Damage>([&](const Damage&) { ++late_calls; });
        }
    });

    bus.publish(Damage{1});
    EXPECT_EQ(late_calls, 0);

    bus.publish(Damage{1});
    EXPECT_EQ(late_calls, 1);
}

TEST(EventBus, ThrowingHandlerIsLoggedAndOthersStillRun) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    EventBus bus;
    int calls = 0;
    bus.subscribe<Damage>([&](const Damage&) { ++calls; });
    bus.subscribe<Damage>([](const Damage&) { throw std::runtime_error("boom"); });
    bus.subscribe<Damage>([&](const Damage&) { ++calls; });

    EXPECT_NO_THROW(bus.publish(Damage{1}));

    EXPECT_EQ(calls, 2);
    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, logger::Level::Error);
    EXPECT_NE(records.front().message.find("boom"), std::string::npos);
}

TEST(EventBus, HandlerThrowingNonStandardExceptionIsLogged) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    EventBus bus;
    int calls = 0;
    bus.subscribe<Damage>([](const Damage&) { throw 7; });
    bus.subscribe<Damage>([&](const Damage&) { ++calls; });

    EXPECT_NO_THROW(bus.publish(Damage{1}));

    EXPECT_EQ(calls, 1);
    EXPECT_EQ(sink->records().size(), 1U);
}

TEST(EventBus, ThrowingHandlerDoesNotStopDispatch) {
    auto sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped(sink, logger::Level::Trace);
    EventBus bus;
    int calls = 0;
    bus.subscribe<Damage>([&](const Damage&) {
        ++calls;
        throw std::runtime_error("boom");
    });

    bus.enqueue(Damage{1});
    bus.enqueue(Damage{2});
    EXPECT_NO_THROW(bus.dispatch());

    EXPECT_EQ(calls, 2);
    EXPECT_EQ(sink->records().size(), 2U);
}

TEST(EventBus, EnqueueIsSafeFromSeveralThreads) {
    EventBus bus;
    int total = 0;
    bus.subscribe<Damage>([&](const Damage& e) { total += e.amount; });
    constexpr int threads = 4;
    constexpr int per_thread = 250;

    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([&] {
            for (int n = 0; n < per_thread; ++n) {
                bus.enqueue(Damage{1});
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    bus.dispatch();

    EXPECT_EQ(total, threads * per_thread);
}

TEST(EventTypeId, IsStableAndDistinctPerType) {
    const auto damage = event::event_type_id(typeid(Damage));
    const auto heal = event::event_type_id(typeid(Heal));

    EXPECT_EQ(damage, event::event_type_id(typeid(Damage)));
    EXPECT_EQ(heal, event::event_type_id(typeid(Heal)));
    EXPECT_NE(damage, heal);
    EXPECT_NE(damage, 0U);
}
