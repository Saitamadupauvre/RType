#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"

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
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::HeartbeatPayload;
using engine::network::Inbound;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::Outbound;
using engine::network::ServerSessions;
using engine::network::ServerSessionsConfig;
using engine::network::TypeEntry;
using namespace std::chrono_literals;

constexpr auto retry_interval = 500ms;
constexpr std::size_t max_attempts = 3;
constexpr auto heartbeat_interval = 1s;
constexpr std::uint32_t sample_nonce = 0xCAFE;

Endpoint endpoint(std::uint16_t port) {
    const auto parsed = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(parsed.has_value());
    return parsed.value_or(Endpoint{});
}

Endpoint server_endpoint() { return endpoint(4242); }

Inbound from(const Endpoint& sender, Message message) {
    return Inbound{.from = sender, .message = {.header = {}, .message = std::move(message)}};
}

Inbound accept_from_server(std::uint32_t nonce = sample_nonce) {
    return from(server_endpoint(),
                ConnectAcceptPayload{.client_nonce = nonce,
                                     .player_entity_id = 3,
                                     .types = {TypeEntry{.type_id = 1, .name = "player"}}});
}

Inbound disconnect_from_server(DisconnectReason reason) {
    return from(server_endpoint(), DisconnectPayload{.reason = reason});
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
                                                       .heartbeat_interval = heartbeat_interval}};
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
    EXPECT_EQ(connection.types(), (std::vector<TypeEntry>{{.type_id = 1, .name = "player"}}));
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

TEST_F(ClientConnectionTest, InputPostponesNextHeartbeat) {
    connect();
    clock.advance(heartbeat_interval - 1ms);

    static_cast<void>(connection.send_input(InputPayload{}));
    clock.advance(heartbeat_interval - 1ms);
    EXPECT_FALSE(connection.update().has_value());

    clock.advance(1ms);
    const auto heartbeat = connection.update();
    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_EQ(heartbeat->message, Message{HeartbeatPayload{}});
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
