#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <string>

#include <gtest/gtest.h>

#include "protocol/Messages.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "stats/TrafficMeter.hpp"

namespace {

using engine::core::time::Duration;
using engine::network::datagram_size;
using engine::network::DisconnectPayload;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::NetworkStats;
using engine::network::ReceivedMessage;
using engine::network::summarize;
using engine::network::TrafficMeter;
using namespace std::chrono_literals;

ReceivedMessage packet(std::uint32_t sequence, std::uint16_t payload_size = 0) {
    return ReceivedMessage{.header = {.sequence = sequence, .payload_size = payload_size},
                           .message = HeartbeatPayload{}};
}

void receive_sequences(TrafficMeter& meter, std::initializer_list<std::uint32_t> sequences) {
    for (const auto sequence : sequences) {
        meter.on_received(packet(sequence), Duration{});
    }
}

} // namespace

TEST(TrafficMeter, DatagramSizeIsHeaderPlusPayload) {
    EXPECT_EQ(datagram_size(HeartbeatPayload{}), 13U);
    EXPECT_EQ(datagram_size(InputPayload{}), 16U);
    EXPECT_EQ(datagram_size(DisconnectPayload{}), 14U);
}

TEST(TrafficMeter, RatesCountTheLastCompleteSecond) {
    TrafficMeter meter;
    meter.on_sent(InputPayload{}, 100ms);
    meter.on_sent(InputPayload{}, 900ms);
    meter.on_received(packet(0, 3), 500ms);

    EXPECT_EQ(meter.stats(999ms).packets_sent_per_second, 0U);
    const auto stats = meter.stats(1s);

    EXPECT_EQ(stats.packets_sent_per_second, 2U);
    EXPECT_EQ(stats.bytes_sent_per_second, 32U);
    EXPECT_EQ(stats.packets_received_per_second, 1U);
    EXPECT_EQ(stats.bytes_received_per_second, 16U);
}

TEST(TrafficMeter, RatesDropToZeroAfterASilentSecond) {
    TrafficMeter meter;
    meter.on_sent(InputPayload{}, 100ms);

    EXPECT_EQ(meter.stats(1500ms).packets_sent_per_second, 1U);
    EXPECT_EQ(meter.stats(2s).packets_sent_per_second, 0U);
}

TEST(TrafficMeter, WindowsFollowTheClockAcrossSeveralSeconds) {
    TrafficMeter meter(10s);
    meter.on_sent(InputPayload{}, 12s + 100ms);
    meter.on_sent(InputPayload{}, 13s + 500ms);

    EXPECT_EQ(meter.stats(14s).packets_sent_per_second, 1U);
}

TEST(TrafficMeter, StatsDoNotChangeTheMeter) {
    TrafficMeter meter;
    meter.on_sent(InputPayload{}, 100ms);

    static_cast<void>(meter.stats(5s));

    EXPECT_EQ(meter.stats(1s).packets_sent_per_second, 1U);
}

TEST(TrafficMeter, CountsGapsInSequencesAsLost) {
    TrafficMeter meter;

    receive_sequences(meter, {0, 1, 4, 5, 9});

    EXPECT_EQ(meter.stats(Duration{}).packets_lost, 5U);
    EXPECT_EQ(meter.stats(Duration{}).packets_reordered, 0U);
}

TEST(TrafficMeter, ALatePacketIsReorderedAndNoLongerLost) {
    TrafficMeter meter;

    receive_sequences(meter, {0, 2, 3, 1});

    const auto stats = meter.stats(Duration{});
    EXPECT_EQ(stats.packets_lost, 0U);
    EXPECT_EQ(stats.packets_reordered, 1U);
}

TEST(TrafficMeter, DuplicatesOfTheNewestPacketAreNeitherLostNorReordered) {
    TrafficMeter meter;

    receive_sequences(meter, {0, 1, 1});

    const auto stats = meter.stats(Duration{});
    EXPECT_EQ(stats.packets_lost, 0U);
    EXPECT_EQ(stats.packets_reordered, 0U);
}

TEST(TrafficMeter, SequencesWrapAround) {
    TrafficMeter meter;

    receive_sequences(meter, {0xFFFF'FFFE, 0xFFFF'FFFF, 1});

    EXPECT_EQ(meter.stats(Duration{}).packets_lost, 1U);
}

TEST(TrafficMeter, KeepsTheLatestRoundTrip) {
    TrafficMeter meter;
    EXPECT_FALSE(meter.stats(Duration{}).round_trip_time.has_value());

    meter.on_round_trip(30ms);
    meter.on_round_trip(25ms);

    EXPECT_EQ(meter.stats(Duration{}).round_trip_time, Duration{25ms});
}

TEST(TrafficMeter, SummaryNamesEveryCounter) {
    const NetworkStats stats{.packets_sent_per_second = 60,
                             .packets_received_per_second = 61,
                             .bytes_sent_per_second = 960,
                             .bytes_received_per_second = 4000,
                             .packets_lost = 2,
                             .packets_reordered = 1,
                             .round_trip_time = 12ms};

    EXPECT_EQ(summarize(stats), "out 60 pkt/s 960 B/s, in 61 pkt/s 4000 B/s, lost 2, "
                                "reordered 1, rtt 12 ms");
    EXPECT_NE(summarize(NetworkStats{}).find("rtt n/a"), std::string::npos);
}
