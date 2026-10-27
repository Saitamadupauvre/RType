#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Events.hpp"

#include "PayloadSamples.hpp"
#include "protocol/Messages.hpp"
#include "session/ServerSessions.hpp"

namespace {

using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EventPayload;
using engine::network::HeartbeatPayload;
using engine::network::Inbound;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::Outbound;
using engine::network::PlayerJoined;
using engine::network::PlayerLeft;
using engine::network::PlayerLeftEvent;
using engine::network::ServerSessions;
using engine::network::ServerSessionsConfig;
using engine::network::StringCategory;
using engine::network::StringTable;
using namespace std::chrono_literals;

constexpr auto session_timeout = 5s;
constexpr std::uint32_t sample_nonce = 0xCAFE;

StringTable player_strings() {
    return payload_samples::strings_of(
        {{.category = StringCategory::type, .id = 1, .name = "player"}});
}

StringTable audio_strings() {
    return payload_samples::strings_of(
        {{.category = StringCategory::sound, .id = 1, .name = "shot.wav"},
         {.category = StringCategory::music, .id = 4, .name = "stage.ogg"}});
}

Endpoint client(std::uint16_t port) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

Inbound from(const Endpoint& endpoint, Message message, std::uint32_t sequence = 0) {
    return Inbound{.from = endpoint,
                   .message = {.header = {.sequence = sequence}, .message = std::move(message)}};
}

Inbound input_from(const Endpoint& endpoint, std::uint32_t sequence, std::int8_t move_x) {
    return from(endpoint, InputPayload{.action_flags = 1, .move_x = move_x, .move_y = 0}, sequence);
}

Inbound connect_from(const Endpoint& endpoint, std::uint32_t nonce = sample_nonce) {
    return from(endpoint, ConnectRequestPayload{.client_nonce = nonce});
}

Message player_left_notice(std::uint32_t player_id) {
    return EventPayload{PlayerLeftEvent{.player_entity_id = player_id}};
}

Message disconnect_with(DisconnectReason reason) { return DisconnectPayload{.reason = reason}; }

class ServerSessionsTest : public testing::Test {
protected:
    ServerSessionsTest() {
        bus.subscribe<PlayerJoined>(
            [this](const PlayerJoined& event) { joined.push_back(event.player_id); });
        bus.subscribe<PlayerLeft>(
            [this](const PlayerLeft& event) { left.push_back(event.player_id); });
    }

    std::uint32_t connect(std::uint16_t port) {
        const auto replies = sessions.handle(connect_from(client(port)));
        EXPECT_EQ(replies.size(), 1U);
        const auto* accept =
            replies.empty() ? nullptr : std::get_if<ConnectAcceptPayload>(&replies.front().message);
        EXPECT_NE(accept, nullptr);
        return accept == nullptr ? 0 : accept->player_entity_id;
    }

    void connect_players(std::uint16_t first_port, std::size_t count) {
        for (std::size_t index = 0; index < count; ++index) {
            connect(static_cast<std::uint16_t>(first_port + index));
        }
    }

    ManualClock clock;
    EventBus bus;
    std::vector<std::uint32_t> joined;
    std::vector<std::uint32_t> left;
    ServerSessions sessions{clock, bus,
                            ServerSessionsConfig{.max_players = 4,
                                                 .timeout = session_timeout,
                                                 .strings = player_strings()}};
};

} // namespace

TEST_F(ServerSessionsTest, AcceptsConnectRequestWithNonceFirstPlayerIdAndTypes) {
    const auto replies = sessions.handle(connect_from(client(5000)));

    ASSERT_EQ(replies.size(), 1U);
    EXPECT_EQ(replies.front().to, client(5000));
    const ConnectAcceptPayload expected{
        .client_nonce = sample_nonce, .player_entity_id = 1, .strings = player_strings()};
    EXPECT_EQ(replies.front().message, Message{expected});
    ASSERT_EQ(sessions.sessions().size(), 1U);
    EXPECT_EQ(sessions.sessions().front().endpoint, client(5000));
}

TEST_F(ServerSessionsTest, PublishesPlayerJoinedOnAccept) {
    connect(5000);
    connect(5001);

    EXPECT_EQ(joined, (std::vector<std::uint32_t>{1, 2}));
}

TEST_F(ServerSessionsTest, DuplicateConnectFromSameEndpointIsIdempotent) {
    const auto first = connect(5000);
    const auto replies = sessions.handle(connect_from(client(5000), 0xBEEF));

    ASSERT_EQ(replies.size(), 1U);
    const auto* accept = std::get_if<ConnectAcceptPayload>(&replies.front().message);
    ASSERT_NE(accept, nullptr);
    EXPECT_EQ(accept->player_entity_id, first);
    EXPECT_EQ(accept->client_nonce, 0xBEEFU);
    EXPECT_EQ(sessions.sessions().size(), 1U);
    EXPECT_EQ(joined.size(), 1U);
}

TEST_F(ServerSessionsTest, RejectsFifthClientWithServerFull) {
    connect_players(5000, 4);

    const auto replies = sessions.handle(connect_from(client(5004)));

    ASSERT_EQ(replies.size(), 1U);
    EXPECT_EQ(replies.front().to, client(5004));
    EXPECT_EQ(replies.front().message, disconnect_with(DisconnectReason::server_full));
    EXPECT_EQ(sessions.sessions().size(), 4U);
    EXPECT_EQ(joined.size(), 4U);
}

TEST(ServerSessions, UsesMaxPlayersGivenByTheGame) {
    ManualClock clock;
    EventBus bus;
    ServerSessions sessions(clock, bus, ServerSessionsConfig{.max_players = 2});

    static_cast<void>(sessions.handle(connect_from(client(5000))));
    static_cast<void>(sessions.handle(connect_from(client(5001))));
    const auto replies = sessions.handle(connect_from(client(5002)));

    ASSERT_EQ(replies.size(), 1U);
    EXPECT_EQ(replies.front().message, disconnect_with(DisconnectReason::server_full));
    EXPECT_EQ(sessions.sessions().size(), 2U);
}

TEST_F(ServerSessionsTest, IgnoresMessagesFromUnknownEndpoint) {
    connect(5000);

    EXPECT_TRUE(sessions.handle(from(client(6000), InputPayload{})).empty());
    EXPECT_TRUE(sessions.handle(from(client(6000), HeartbeatPayload{})).empty());
    EXPECT_TRUE(
        sessions.handle(from(client(6000), disconnect_with(DisconnectReason::normal))).empty());
    EXPECT_EQ(sessions.sessions().size(), 1U);
    EXPECT_TRUE(left.empty());
}

TEST_F(ServerSessionsTest, DisconnectFreesSlotAndNotifiesRemainingPlayers) {
    connect_players(5000, 3);

    const auto replies =
        sessions.handle(from(client(5001), disconnect_with(DisconnectReason::normal)));

    ASSERT_EQ(replies.size(), 2U);
    EXPECT_EQ(replies[0].to, client(5000));
    EXPECT_EQ(replies[1].to, client(5002));
    for (const auto& reply : replies) {
        EXPECT_EQ(reply.message, player_left_notice(2));
    }
    EXPECT_EQ(sessions.sessions().size(), 2U);
    EXPECT_EQ(left, (std::vector<std::uint32_t>{2}));
}

TEST_F(ServerSessionsTest, SlotIsReusableAfterDisconnect) {
    connect_players(5000, 4);
    static_cast<void>(
        sessions.handle(from(client(5002), disconnect_with(DisconnectReason::normal))));

    EXPECT_EQ(connect(5004), 3U);
    EXPECT_EQ(sessions.sessions().size(), 4U);
}

TEST_F(ServerSessionsTest, KeepsClientUntilTimeoutHasFullyElapsed) {
    connect(5000);

    clock.advance(session_timeout);

    EXPECT_TRUE(sessions.drop_timed_out().empty());
    EXPECT_EQ(sessions.sessions().size(), 1U);
}

TEST_F(ServerSessionsTest, RemovesSilentClientAfterTimeoutAndNotifiesOthers) {
    connect(5000);
    clock.advance(1s);
    connect(5001);
    clock.advance(session_timeout);

    const auto replies = sessions.drop_timed_out();

    ASSERT_EQ(replies.size(), 2U);
    EXPECT_EQ(replies[0].to, client(5000));
    EXPECT_EQ(replies[0].message, disconnect_with(DisconnectReason::timeout));
    EXPECT_EQ(replies[1].to, client(5001));
    EXPECT_EQ(replies[1].message, player_left_notice(1));
    ASSERT_EQ(sessions.sessions().size(), 1U);
    EXPECT_EQ(sessions.sessions().front().player_id, 2U);
    EXPECT_EQ(left, (std::vector<std::uint32_t>{1}));
}

TEST_F(ServerSessionsTest, AnyValidPacketRefreshesLastSeen) {
    connect(5000);
    clock.advance(4s);
    static_cast<void>(sessions.handle(from(client(5000), InputPayload{})));
    clock.advance(4s);
    static_cast<void>(sessions.handle(from(client(5000), HeartbeatPayload{})));
    clock.advance(4s);

    EXPECT_TRUE(sessions.drop_timed_out().empty());
    EXPECT_EQ(sessions.sessions().size(), 1U);
}

TEST_F(ServerSessionsTest, ClientsTimingOutTogetherAreNotNotifiedAboutEachOther) {
    connect_players(5000, 2);
    clock.advance(session_timeout + 1ns);

    const auto replies = sessions.drop_timed_out();

    ASSERT_EQ(replies.size(), 2U);
    for (const auto& reply : replies) {
        EXPECT_EQ(reply.message, disconnect_with(DisconnectReason::timeout));
    }
    EXPECT_TRUE(sessions.sessions().empty());
    EXPECT_EQ(left, (std::vector<std::uint32_t>{1, 2}));
}

TEST_F(ServerSessionsTest, BroadcastAddressesEverySession) {
    connect_players(5000, 3);
    const Message message = HeartbeatPayload{};

    const auto outbound = sessions.broadcast(message);

    ASSERT_EQ(outbound.size(), 3U);
    for (std::size_t index = 0; index < outbound.size(); ++index) {
        EXPECT_EQ(outbound[index].to, client(static_cast<std::uint16_t>(5000 + index)));
        EXPECT_EQ(outbound[index].message, message);
    }
}

TEST_F(ServerSessionsTest, BroadcastWithoutSessionsSendsNothing) {
    EXPECT_TRUE(sessions.broadcast(HeartbeatPayload{}).empty());
}

TEST_F(ServerSessionsTest, NewSessionStartsWithIdleInput) {
    connect(5000);

    EXPECT_EQ(sessions.sessions().front().input, InputPayload{});
}

TEST_F(ServerSessionsTest, StoresLatestInputOfEachSession) {
    connect_players(5000, 2);

    EXPECT_TRUE(sessions.handle(input_from(client(5000), 3, 1)).empty());
    static_cast<void>(sessions.handle(input_from(client(5001), 3, -1)));
    static_cast<void>(sessions.handle(input_from(client(5000), 4, -1)));

    EXPECT_EQ(sessions.sessions()[0].input.move_x, -1);
    EXPECT_EQ(sessions.sessions()[0].input.action_flags, 1);
    EXPECT_EQ(sessions.sessions()[1].input.move_x, -1);
}

TEST_F(ServerSessionsTest, IgnoresOutOfOrderInput) {
    connect(5000);

    static_cast<void>(sessions.handle(input_from(client(5000), 10, 1)));
    static_cast<void>(sessions.handle(input_from(client(5000), 9, -1)));

    EXPECT_EQ(sessions.sessions().front().input.move_x, 1);
}

TEST_F(ServerSessionsTest, IgnoresDuplicateInput) {
    connect(5000);

    static_cast<void>(sessions.handle(input_from(client(5000), 10, 1)));
    static_cast<void>(sessions.handle(input_from(client(5000), 10, -1)));

    EXPECT_EQ(sessions.sessions().front().input.move_x, 1);
}

TEST_F(ServerSessionsTest, AcceptsInputAcrossSequenceWrap) {
    connect(5000);

    static_cast<void>(sessions.handle(input_from(client(5000), 0xFFFF'FFFF, 1)));
    static_cast<void>(sessions.handle(input_from(client(5000), 0, -1)));

    EXPECT_EQ(sessions.sessions().front().input.move_x, -1);
}

TEST_F(ServerSessionsTest, OlderInputStillKeepsSessionAlive) {
    connect(5000);
    static_cast<void>(sessions.handle(input_from(client(5000), 10, 1)));
    clock.advance(4s);

    static_cast<void>(sessions.handle(input_from(client(5000), 9, -1)));
    clock.advance(4s);

    EXPECT_TRUE(sessions.drop_timed_out().empty());
}

TEST_F(ServerSessionsTest, IgnoresInputFromUnknownEndpoint) {
    connect(5000);

    EXPECT_TRUE(sessions.handle(input_from(client(6000), 1, 1)).empty());

    ASSERT_EQ(sessions.sessions().size(), 1U);
    EXPECT_EQ(sessions.sessions().front().input, InputPayload{});
}

TEST_F(ServerSessionsTest, ReconnectedSessionAcceptsInputWithLowerSequence) {
    connect(5000);
    static_cast<void>(sessions.handle(input_from(client(5000), 500, 1)));
    static_cast<void>(
        sessions.handle(from(client(5000), disconnect_with(DisconnectReason::normal))));
    connect(5000);

    static_cast<void>(sessions.handle(input_from(client(5000), 2, -1)));

    EXPECT_EQ(sessions.sessions().front().input.move_x, -1);
}

TEST_F(ServerSessionsTest, AnswersEveryHeartbeatWithAHeartbeat) {
    connect(5000);

    const auto replies = sessions.handle(from(client(5000), HeartbeatPayload{}, 1));

    ASSERT_EQ(replies.size(), 1U);
    EXPECT_EQ(replies.front().to, client(5000));
    EXPECT_EQ(replies.front().message, Message{HeartbeatPayload{}});
}

TEST_F(ServerSessionsTest, NumbersPacketsSeparatelyForEachSession) {
    connect(5000);
    connect(5001);

    const auto first = sessions.broadcast(HeartbeatPayload{});
    const auto second = sessions.broadcast(HeartbeatPayload{});

    ASSERT_EQ(first.size(), 2U);
    ASSERT_EQ(second.size(), 2U);
    EXPECT_EQ(first[0].sequence, 1U);
    EXPECT_EQ(first[1].sequence, 1U);
    EXPECT_EQ(second[0].sequence, 2U);
    EXPECT_EQ(second[1].sequence, 2U);
}

TEST_F(ServerSessionsTest, ReportsTrafficOfEachPlayer) {
    connect(5000);
    connect(5001);
    for (const std::uint32_t sequence : {1U, 2U, 5U}) {
        static_cast<void>(sessions.handle(input_from(client(5001), sequence, 0)));
    }
    clock.advance(1s);

    const auto stats = sessions.stats();

    ASSERT_EQ(stats.size(), 2U);
    EXPECT_EQ(stats[0].player_id, 1U);
    EXPECT_EQ(stats[0].network.packets_received_per_second, 1U);
    EXPECT_EQ(stats[0].network.packets_sent_per_second, 1U);
    EXPECT_EQ(stats[0].network.packets_lost, 0U);
    EXPECT_EQ(stats[1].player_id, 2U);
    EXPECT_EQ(stats[1].network.packets_received_per_second, 4U);
    EXPECT_EQ(stats[1].network.packets_lost, 2U);
    EXPECT_FALSE(stats[1].network.round_trip_time.has_value());
}

TEST(ServerSessionsAudio, AcceptCarriesTheStringTableAndCurrentMusic) {
    ManualClock clock;
    EventBus bus;
    ServerSessions sessions(clock, bus,
                            ServerSessionsConfig{.max_players = 1, .strings = audio_strings()});

    sessions.set_current_music(4);
    const auto replies = sessions.handle(connect_from(client(5000)));

    ASSERT_EQ(replies.size(), 1U);
    const auto* accept = std::get_if<ConnectAcceptPayload>(&replies.front().message);
    ASSERT_NE(accept, nullptr);
    EXPECT_EQ(accept->strings, audio_strings());
    EXPECT_EQ(accept->current_music, 4);
}

TEST(ServerSessionsAudio, IgnoresAnUnknownCurrentMusic) {
    ManualClock clock;
    EventBus bus;
    ServerSessions sessions(clock, bus,
                            ServerSessionsConfig{.max_players = 1, .strings = audio_strings()});

    sessions.set_current_music(4);
    sessions.set_current_music(9);
    const auto playing = sessions.handle(connect_from(client(5000)));
    sessions.set_current_music(engine::network::no_music);
    const auto stopped = sessions.handle(connect_from(client(5000)));

    ASSERT_EQ(playing.size(), 1U);
    ASSERT_EQ(stopped.size(), 1U);
    EXPECT_EQ(std::get<ConnectAcceptPayload>(playing.front().message).current_music, 4);
    EXPECT_EQ(std::get<ConnectAcceptPayload>(stopped.front().message).current_music,
              engine::network::no_music);
}
