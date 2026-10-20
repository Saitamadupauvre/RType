#include <algorithm>
#include <cstddef>
#include <numeric>
#include <stop_token>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "thread/MessageQueue.hpp"

using engine::network::MessageQueue;

namespace {

constexpr int producer_count = 4;
constexpr int values_per_producer = 10'000;
constexpr int total_values = producer_count * values_per_producer;

std::vector<std::jthread> start_producers(MessageQueue<int>& queue) {
    std::vector<std::jthread> producers;
    producers.reserve(producer_count);
    for (int producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&queue, producer] {
            for (int index = 0; index < values_per_producer; ++index) {
                queue.push(producer * values_per_producer + index);
            }
        });
    }
    return producers;
}

std::vector<int> all_values() {
    std::vector<int> values(total_values);
    std::iota(values.begin(), values.end(), 0);
    return values;
}

} // namespace

TEST(MessageQueue, DrainOnEmptyQueueReturnsEmptyVector) {
    MessageQueue<int> queue;

    EXPECT_TRUE(queue.drain().empty());
}

TEST(MessageQueue, DrainKeepsPushOrderForSingleProducer) {
    MessageQueue<int> queue;

    queue.push(3);
    queue.push(1);
    queue.push(2);

    EXPECT_EQ(queue.drain(), (std::vector<int>{3, 1, 2}));
}

TEST(MessageQueue, DrainEmptiesTheQueue) {
    MessageQueue<int> queue;
    queue.push(1);

    static_cast<void>(queue.drain());

    EXPECT_TRUE(queue.drain().empty());
}

TEST(MessageQueue, FullQueueRejectsPushAndKeepsQueuedItems) {
    MessageQueue<int> queue(2);

    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_FALSE(queue.push(3));

    EXPECT_EQ(queue.drain(), (std::vector<int>{1, 2}));
}

TEST(MessageQueue, DrainMakesRoomInFullQueue) {
    MessageQueue<int> queue(1);
    queue.push(1);

    static_cast<void>(queue.drain());

    EXPECT_TRUE(queue.push(2));
    EXPECT_EQ(queue.drain(), (std::vector<int>{2}));
}

TEST(MessageQueue, ConcurrentProducersDeliverEveryValueExactlyOnce) {
    MessageQueue<int> queue;

    auto producers = start_producers(queue);
    producers.clear();
    auto drained = queue.drain();

    std::ranges::sort(drained);
    EXPECT_EQ(drained, all_values());
}

TEST(MessageQueue, DrainingWhileProducersPushLosesNothing) {
    MessageQueue<int> queue;
    std::vector<int> received;
    received.reserve(total_values);

    auto producers = start_producers(queue);
    while (received.size() < static_cast<std::size_t>(total_values)) {
        auto batch = queue.drain();
        received.insert(received.end(), batch.begin(), batch.end());
    }
    producers.clear();

    EXPECT_TRUE(queue.drain().empty());
    std::ranges::sort(received);
    EXPECT_EQ(received, all_values());
}

TEST(MessageQueue, WaitAndDrainReturnsItemsAlreadyQueued) {
    MessageQueue<int> queue;
    const std::stop_source never_stopped;
    queue.push(1);
    queue.push(2);

    EXPECT_EQ(queue.wait_and_drain(never_stopped.get_token()), (std::vector<int>{1, 2}));
    EXPECT_TRUE(queue.drain().empty());
}

TEST(MessageQueue, WaitAndDrainWakesUpWhenAnotherThreadPushes) {
    MessageQueue<int> queue;
    std::vector<int> received;

    std::jthread consumer([&queue, &received](std::stop_token stop_token) {
        received = queue.wait_and_drain(stop_token);
    });
    queue.push(7);
    consumer.join();

    EXPECT_EQ(received, (std::vector<int>{7}));
}

TEST(MessageQueue, WaitAndDrainReturnsEmptyWhenStopIsRequested) {
    MessageQueue<int> queue;
    std::vector<int> received{-1};

    std::jthread consumer([&queue, &received](std::stop_token stop_token) {
        received = queue.wait_and_drain(stop_token);
    });
    consumer.request_stop();
    consumer.join();

    EXPECT_TRUE(received.empty());
}

TEST(MessageQueue, WaitAndDrainReturnsQueuedItemsEvenWhenStopIsRequested) {
    MessageQueue<int> queue;
    std::stop_source stop_source;
    queue.push(3);

    stop_source.request_stop();

    EXPECT_EQ(queue.wait_and_drain(stop_source.get_token()), (std::vector<int>{3}));
}
