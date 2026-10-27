#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"

#include "PayloadSamples.hpp"
#include "protocol/Messages.hpp"
#include "session/ClientConnection.hpp"
#include "session/ServerSessions.hpp"

namespace {

using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ClientConnection;
using engine::network::ClientConnectionConfig;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectionState;
using engine::network::ConnectRequestPayload;
using engine::network::DestroyEvent;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EntityState;
using engine::network::EventPayload;
using engine::network::HeartbeatPayload;
using engine::network::Inbound;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::Outbound;
using engine::network::ServerSessions;
using engine::network::ServerSessionsConfig;
using engine::network::SnapshotPayload;
using engine::network::SpawnEvent;
using engine::network::StringCategory;
using namespace std::chrono_literals;

constexpr auto retry_interval = 500ms;
constexpr std::size_t max_attempts = 3;
constexpr auto heartbeat_interval = 1s;
constexpr auto server_timeout = 3s;
constexpr std::uint32_t sample_nonce = 0xCAFE;

Endpoint endpoint(std::uint16_t port) {
    const auto parsed = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(parsed.has_value());
    return parsed.value_or(Endpoint{});
}

Endpoint server_endpoint() { return endpoint(4242); }

Inbound from(const Endpoint& sender, Message message, std::uint32_t sequence = 0) {
    return Inbound{.from = sender,
                   .message = {.header = {.sequence = sequence}, .message = std::move(message)}};
}

Inbound accept_from_server(std::uint32_t nonce = sample_nonce) {
    return from(
        server_endpoint(),
        ConnectAcceptPayload{.client_nonce = nonce,
                             .player_entity_id = 3,
                             .strings = payload_samples::strings_of(
                                 {{.category = StringCategory::type, .id = 1, .name = "player"}})});
}

Inbound disconnect_from_server(DisconnectReason reason) {
    return from(server_endpoint(), DisconnectPayload{.reason = reason});
}

Message snapshot_of_type(std::uint16_t type_id) {
    return SnapshotPayload{
        .server_tick = 1,
        .entities = {
            EntityState{.entity_id = 1, .type_id = type_id, .x = 0, .y = 0, .vx = 0, .vy = 0}}};
}

Message spawn_of_type(std::uint16_t type_id) {
    return EventPayload{SpawnEvent{.entity_id = 1, .type_id = type_id, .x = 0, .y = 0}};
}

Message connect_request() { return ConnectRequestPayload{.client_nonce = sample_nonce}; }

class ClientConnectionTest : public testing::Test {
protected:
    void connect() {
        static_cast<void>(connection.update());
        connection.handle(accept_from_server());
        ASSERT_EQ(connection.state(), ConnectionState::connected);
    }

    ManualClock clock;
    ClientConnection connection{clock, server_endpoint(), sample_nonce,
                                ClientConnectionConfig{.retry_interval = retry_interval,
                                                       .max_attempts = max_attempts,
                                                       .heartbeat_interval = heartbeat_interval,
                                                       .server_timeout = server_timeout}};
};

} // namespace

TEST_F(ClientConnectionTest, SendsConnectRequestToServerOnFirstUpdate) {
    const auto outbound = connection.update();

    ASSERT_TRUE(outbound.has_value());
    EXPECT_EQ(outbound->to, server_endpoint());
    EXPECT_EQ(outbound->message, connect_request());
    EXPECT_EQ(connection.state(), ConnectionState::connecting);
}

TEST_F(ClientConnectionTest, RetriesOnlyOnceTheRetryIntervalHasElapsed) {
    static_cast<void>(connection.update());

    clock.advance(retry_interval - 1ns);
    EXPECT_FALSE(connection.update().has_value());

    clock.advance(1ns);
    const auto retry = connection.update();
    ASSERT_TRUE(retry.has_value());
    EXPECT_EQ(retry->message, connect_request());
}

TEST_F(ClientConnectionTest, GivesUpAfterRetryLimit) {
    for (std::size_t attempt = 0; attempt < max_attempts; ++attempt) {
        EXPECT_TRUE(connection.update().has_value()) << "attempt " << attempt;
        clock.advance(retry_interval);
    }

    EXPECT_FALSE(connection.update().has_value());
    EXPECT_EQ(connection.state(), ConnectionState::gave_up);
    clock.advance(retry_interval);
    EXPECT_FALSE(connection.update().has_value());
}

TEST_F(ClientConnectionTest, ConnectsOnAcceptWithMatchingNonce) {
    static_cast<void>(connection.update());

    connection.handle(accept_from_server());

    EXPECT_EQ(connection.state(), ConnectionState::connected);
    EXPECT_EQ(connection.player_id(), 3U);
    EXPECT_TRUE(connection.strings().is_known_type(1));
    EXPECT_FALSE(connection.strings().is_known_type(2));
}

TEST_F(ClientConnectionTest, DropsSnapshotsAndEventsBeforeConnectAccept) {
    static_cast<void>(connection.update());

    EXPECT_FALSE(connection.handle(from(server_endpoint(), snapshot_of_type(1))));
    EXPECT_FALSE(connection.handle(from(server_endpoint(), EventPayload{DestroyEvent{}})));
}

TEST_F(ClientConnectionTest, AcceptsSnapshotsAndSpawnsWithKnownTypes) {
    connect();

    EXPECT_TRUE(connection.handle(from(server_endpoint(), snapshot_of_type(1))));
    EXPECT_TRUE(connection.handle(from(server_endpoint(), spawn_of_type(1))));
    EXPECT_TRUE(connection.handle(from(server_endpoint(), EventPayload{DestroyEvent{}})));
}

TEST_F(ClientConnectionTest, DropsAWholeSnapshotWithOneUnknownType) {
    connect();
    auto snapshot = snapshot_of_type(1);
    std::get<SnapshotPayload>(snapshot).entities.push_back(
        EntityState{.entity_id = 2, .type_id = 2, .x = 0, .y = 0, .vx = 0, .vy = 0});

    EXPECT_FALSE(connection.handle(from(server_endpoint(), snapshot)));
    EXPECT_EQ(connection.state(), ConnectionState::connected);
}

TEST_F(ClientConnectionTest, DropsASpawnWithAnUnknownType) {
    connect();

    EXPECT_FALSE(connection.handle(from(server_endpoint(), spawn_of_type(2))));
    EXPECT_EQ(connection.state(), ConnectionState::connected);
}

TEST_F(ClientConnectionTest, IgnoresAcceptWithAnotherNonce) {
    static_cast<void>(connection.update());

    connection.handle(accept_from_server(0xBEEF));

    EXPECT_EQ(connection.state(), ConnectionState::connecting);
}

TEST_F(ClientConnectionTest, IgnoresMessagesFromAnotherEndpoint) {
    static_cast<void>(connection.update());

    connection.handle(from(endpoint(6000), accept_from_server().message.message));
    connection.handle(
        from(endpoint(6000), DisconnectPayload{.reason = DisconnectReason::server_full}));

    EXPECT_EQ(connection.state(), ConnectionState::connecting);
}

TEST_F(ClientConnectionTest, IsRejectedWhenServerIsFull) {
    static_cast<void>(connection.update());

    connection.handle(disconnect_from_server(DisconnectReason::server_full));

    EXPECT_EQ(connection.state(), ConnectionState::rejected);
    clock.advance(retry_interval);
    EXPECT_FALSE(connection.update().has_value());
}

TEST_F(ClientConnectionTest, SendsHeartbeatsAtFixedIntervalOnceConnected) {
    connect();

    clock.advance(heartbeat_interval - 1ns);
    EXPECT_FALSE(connection.update().has_value());

    clock.advance(1ns);
    const auto heartbeat = connection.update();
    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_EQ(heartbeat->to, server_endpoint());
    EXPECT_EQ(heartbeat->message, Message{HeartbeatPayload{}});
    EXPECT_FALSE(connection.update().has_value());
}

TEST_F(ClientConnectionTest, SendsInputOnlyWhileConnected) {
    const InputPayload input{.action_flags = 1, .move_x = -1, .move_y = 1};
    EXPECT_FALSE(connection.send_input(input).has_value());

    connect();
    const auto outbound = connection.send_input(input);

    ASSERT_TRUE(outbound.has_value());
    EXPECT_EQ(outbound->to, server_endpoint());
    EXPECT_EQ(outbound->message, Message{input});
    static_cast<void>(connection.disconnect());
    EXPECT_FALSE(connection.send_input(input).has_value());
}

TEST_F(ClientConnectionTest, HeartbeatsKeepTheirPaceWhileSendingInputs) {
    connect();
    clock.advance(heartbeat_interval - 1ms);
    static_cast<void>(connection.send_input(InputPayload{}));

    clock.advance(1ms);
    const auto heartbeat = connection.update();

    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_EQ(heartbeat->message, Message{HeartbeatPayload{}});
}

TEST_F(ClientConnectionTest, NumbersEveryPacketToTheServerInOrder) {
    const auto request = connection.update();
    connection.handle(accept_from_server());
    const auto input = connection.send_input(InputPayload{});
    const auto goodbye = connection.disconnect();

    ASSERT_TRUE(request && input && goodbye);
    EXPECT_EQ(request->sequence, 0U);
    EXPECT_EQ(input->sequence, 1U);
    EXPECT_EQ(goodbye->sequence, 2U);
}

TEST_F(ClientConnectionTest, MeasuresRoundTripFromTheHeartbeatAnswer) {
    connect();
    EXPECT_FALSE(connection.stats().round_trip_time.has_value());
    clock.advance(heartbeat_interval);
    ASSERT_TRUE(connection.update().has_value());

    clock.advance(40ms);
    connection.handle(from(server_endpoint(), HeartbeatPayload{}));

    EXPECT_EQ(connection.stats().round_trip_time, 40ms);
}

TEST_F(ClientConnectionTest, IgnoresAHeartbeatAnswerWithoutAPendingHeartbeat) {
    connect();

    connection.handle(from(server_endpoint(), HeartbeatPayload{}));

    EXPECT_FALSE(connection.stats().round_trip_time.has_value());
}

TEST_F(ClientConnectionTest, CountsTrafficWithTheServer) {
    connect();
    clock.advance(heartbeat_interval);
    static_cast<void>(connection.update());
    connection.handle(from(server_endpoint(), HeartbeatPayload{}, 1));

    clock.advance(heartbeat_interval);
    const auto stats = connection.stats();

    EXPECT_EQ(stats.packets_sent_per_second, 1U);
    EXPECT_EQ(stats.packets_received_per_second, 1U);
    EXPECT_EQ(stats.bytes_sent_per_second, 13U);
    EXPECT_EQ(stats.bytes_received_per_second, 13U);
    EXPECT_EQ(stats.packets_lost, 0U);
}

TEST_F(ClientConnectionTest, ServerDisconnectEndsConnection) {
    connect();

    connection.handle(disconnect_from_server(DisconnectReason::server_shutdown));

    EXPECT_EQ(connection.state(), ConnectionState::disconnected);
    clock.advance(heartbeat_interval);
    EXPECT_FALSE(connection.update().has_value());
}

TEST_F(ClientConnectionTest, DisconnectSendsNormalDisconnectOnlyOnce) {
    connect();

    const auto goodbye = connection.disconnect();

    ASSERT_TRUE(goodbye.has_value());
    EXPECT_EQ(goodbye->to, server_endpoint());
    EXPECT_EQ(goodbye->message, Message{DisconnectPayload{.reason = DisconnectReason::normal}});
    EXPECT_EQ(connection.state(), ConnectionState::disconnected);
    EXPECT_FALSE(connection.disconnect().has_value());
}

TEST_F(ClientConnectionTest, HasNoDisconnectReasonWhileLive) {
    EXPECT_FALSE(connection.disconnect_reason().has_value());

    connect();

    EXPECT_FALSE(connection.disconnect_reason().has_value());
}

TEST_F(ClientConnectionTest, ReportsEachReasonSentByTheServer) {
    for (const auto reason :
         {DisconnectReason::normal, DisconnectReason::timeout, DisconnectReason::server_shutdown,
          DisconnectReason::protocol_error, DisconnectReason::server_full}) {
        ClientConnection client(clock, server_endpoint(), sample_nonce);
        static_cast<void>(client.update());
        client.handle(accept_from_server());

        client.handle(disconnect_from_server(reason));

        EXPECT_EQ(client.state(), ConnectionState::disconnected) << static_cast<int>(reason);
        EXPECT_EQ(client.disconnect_reason(), reason);
    }
}

TEST_F(ClientConnectionTest, ReportsRejectionAsServerFull) {
    static_cast<void>(connection.update());

    connection.handle(disconnect_from_server(DisconnectReason::server_full));

    EXPECT_EQ(connection.disconnect_reason(), DisconnectReason::server_full);
}

TEST_F(ClientConnectionTest, ReportsGivingUpAsTimeout) {
    for (std::size_t attempt = 0; attempt <= max_attempts; ++attempt) {
        static_cast<void>(connection.update());
        clock.advance(retry_interval);
    }

    EXPECT_EQ(connection.state(), ConnectionState::gave_up);
    EXPECT_EQ(connection.disconnect_reason(), DisconnectReason::timeout);
}

TEST_F(ClientConnectionTest, ReportsOwnDisconnectAsNormal) {
    connect();

    static_cast<void>(connection.disconnect());

    EXPECT_EQ(connection.disconnect_reason(), DisconnectReason::normal);
}

TEST_F(ClientConnectionTest, TimesOutWhenTheServerStaysSilent) {
    connect();

    clock.advance(server_timeout);
    EXPECT_TRUE(connection.update().has_value());
    EXPECT_EQ(connection.state(), ConnectionState::connected);

    clock.advance(1ns);
    EXPECT_FALSE(connection.update().has_value());
    EXPECT_EQ(connection.state(), ConnectionState::disconnected);
    EXPECT_EQ(connection.disconnect_reason(), DisconnectReason::timeout);
}

TEST_F(ClientConnectionTest, AnyPacketFromTheServerPostponesTheTimeout) {
    connect();
    clock.advance(server_timeout);

    EXPECT_TRUE(connection.handle(
        from(server_endpoint(), SnapshotPayload{.server_tick = 1, .entities = {}})));
    clock.advance(server_timeout);
    static_cast<void>(connection.update());

    EXPECT_EQ(connection.state(), ConnectionState::connected);
}

TEST_F(ClientConnectionTest, PacketsFromAnotherEndpointDoNotPostponeTheTimeout) {
    connect();
    clock.advance(server_timeout);

    EXPECT_FALSE(
        connection.handle(from(endpoint(6000), SnapshotPayload{.server_tick = 1, .entities = {}})));
    clock.advance(1ns);
    static_cast<void>(connection.update());

    EXPECT_EQ(connection.state(), ConnectionState::disconnected);
}

TEST_F(ClientConnectionTest, IgnoresPacketsAfterTheConnectionEnded) {
    connect();
    static_cast<void>(connection.disconnect());

    EXPECT_FALSE(connection.handle(accept_from_server()));
    EXPECT_FALSE(connection.handle(disconnect_from_server(DisconnectReason::server_shutdown)));

    EXPECT_EQ(connection.state(), ConnectionState::disconnected);
    EXPECT_EQ(connection.disconnect_reason(), DisconnectReason::normal);
    clock.advance(server_timeout * 2);
    EXPECT_FALSE(connection.update().has_value());
}

TEST(ClientConnection, CompletesHandshakeAndDisconnectWithServerSessions) {
    ManualClock clock;
    EventBus bus;
    ServerSessions server(clock, bus, ServerSessionsConfig{.max_players = 1});
    const auto client_endpoint = endpoint(5000);
    ClientConnection client(clock, server_endpoint(), sample_nonce);
    const auto deliver_to_server = [&](const Outbound& outbound) {
        EXPECT_EQ(outbound.to, server_endpoint());
        return server.handle(from(client_endpoint, outbound.message));
    };

    const auto request = client.update();
    ASSERT_TRUE(request.has_value());
    for (const auto& reply : deliver_to_server(*request)) {
        EXPECT_EQ(reply.to, client_endpoint);
        client.handle(from(server_endpoint(), reply.message));
    }
    ASSERT_EQ(client.state(), ConnectionState::connected);
    EXPECT_EQ(client.player_id(), 1U);

    const auto goodbye = client.disconnect();
    ASSERT_TRUE(goodbye.has_value());
    EXPECT_TRUE(deliver_to_server(*goodbye).empty());
    EXPECT_TRUE(server.sessions().empty());
}
