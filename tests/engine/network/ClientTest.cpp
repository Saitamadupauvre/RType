#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"

#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Velocity;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::core::time::SteadyClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectionState;
using engine::network::ConnectRequestPayload;
using engine::network::Disconnected;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::NetworkClient;
using engine::network::NetworkServer;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

constexpr auto delivery_deadline = 2s;

Endpoint loopback(std::uint16_t port) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

bool wait_until(const std::function<void()>& step, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        step();
        std::this_thread::yield();
    }
    return true;
}

struct FromClient {
    Endpoint from;
    ReceivedMessage message;
};

class FakeServer {
public:
    FakeServer() {
        std::error_code ec;
        socket_.bind(loopback(0), ec);
        EXPECT_FALSE(ec) << ec.message();
    }

    [[nodiscard]] std::uint16_t port() const {
        std::error_code ec;
        return socket_.local_endpoint(ec).port();
    }

    std::optional<FromClient> receive() {
        std::array<std::uint8_t, max_datagram_size> buffer{};
        Endpoint from;
        std::error_code ec;
        const auto size = socket_.receive_from(buffer, from, delivery_deadline, ec);
        if (ec) {
            return std::nullopt;
        }
        auto message = ProtocolHandler{}.decode(std::span(buffer).first(size));
        if (!message) {
            return std::nullopt;
        }
        return FromClient{.from = from, .message = std::move(*message)};
    }

    void send(const Endpoint& to, const Message& message) {
        const auto bytes = ProtocolHandler{}.encode(message, 0);
        ASSERT_TRUE(bytes.has_value());
        std::error_code ec;
        socket_.send_to(*bytes, to, ec);
        ASSERT_FALSE(ec) << ec.message();
    }

private:
    UdpSocket socket_{AF_INET};
};

class NetworkClientTest : public testing::Test {
protected:
    NetworkClientTest() {
        bus.subscribe<Disconnected>(
            [this](const Disconnected& event) { reasons.push_back(event.reason); });
    }

    void start_server() {
        std::error_code ec;
        ASSERT_TRUE(server.start(ec)) << ec.message();
    }

    void connect(NetworkClient& joining) {
        std::error_code ec;
        ASSERT_TRUE(joining.connect("127.0.0.1", server.port(), ec)) << ec.message();
        ASSERT_TRUE(wait_until(
            [&] {
                server.receive();
                joining.update();
            },
            [&] { return joining.state() != ConnectionState::connecting; }));
    }

    SteadyClock clock;
    EventBus bus;
    std::vector<DisconnectReason> reasons;
    NetworkServer server{clock, bus, ServerOptions{.port = 0, .max_players = 1}};
    NetworkClient client{clock, bus};
};

} // namespace

TEST_F(NetworkClientTest, IsDisconnectedAndInertBeforeConnecting) {
    client.update();
    client.send_input(1, 1, 1);
    client.disconnect();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(client.player_id(), 0U);
    EXPECT_FALSE(client.server_tick().has_value());
    EXPECT_TRUE(client.entities().empty());
    EXPECT_TRUE(reasons.empty());
}

TEST_F(NetworkClientTest, ConnectRejectsANonNumericAddress) {
    std::error_code ec;

    EXPECT_FALSE(client.connect("not an address", 4242, ec));

    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_EQ(client.state(), ConnectionState::disconnected);
}

TEST_F(NetworkClientTest, ConnectsOnlyOnce) {
    std::error_code ec;
    ASSERT_TRUE(client.connect("127.0.0.1", 4242, ec)) << ec.message();

    EXPECT_FALSE(client.connect("127.0.0.1", 4242, ec));

    EXPECT_EQ(ec, std::errc::already_connected);
}

TEST_F(NetworkClientTest, ReceivesTheWorldAndSendsInputs) {
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    registry.register_component<Velocity>();
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 2});
    registry.add(entity, Position{30.0F, -40.0F});
    registry.add(entity, Velocity{1.0F, 0.0F});
    start_server();

    connect(client);
    ASSERT_EQ(client.state(), ConnectionState::connected);
    EXPECT_EQ(client.player_id(), 1U);

    client.send_input(1, -1, 1);
    ASSERT_TRUE(wait_until(
        [this] { server.receive(); },
        [this] { return !server.inputs().empty() && server.inputs().front().move_x == -1; }));
    EXPECT_EQ(server.inputs().front().move_y, 1);

    server.send_snapshot(registry);
    ASSERT_TRUE(
        wait_until([this] { client.update(); }, [this] { return !client.entities().empty(); }));
    EXPECT_EQ(client.server_tick(), 1U);
    const auto received = client.entities().front();
    EXPECT_EQ(received.type_id, 2);
    EXPECT_FLOAT_EQ(received.x, 30.0F);
    EXPECT_FLOAT_EQ(received.y, -40.0F);
    EXPECT_FLOAT_EQ(received.vx, 1.0F);
    EXPECT_TRUE(reasons.empty());
}

TEST_F(NetworkClientTest, ReportsServerFullWhenRejected) {
    start_server();
    connect(client);
    NetworkClient second(clock, bus);

    connect(second);

    EXPECT_EQ(second.state(), ConnectionState::rejected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::server_full}));
}

TEST_F(NetworkClientTest, ReportsServerShutdownOnceAndKeepsTheLastWorld) {
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 1});
    registry.add(entity, Position{5.0F, 6.0F});
    start_server();
    connect(client);
    server.send_snapshot(registry);
    ASSERT_TRUE(
        wait_until([this] { client.update(); }, [this] { return !client.entities().empty(); }));

    server.stop();

    ASSERT_TRUE(wait_until([this] { client.update(); },
                           [this] { return client.state() != ConnectionState::connected; }));
    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(client.entities().size(), 1U);
    client.update();
    client.disconnect();
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::server_shutdown}));
}

TEST_F(NetworkClientTest, DisconnectTellsTheServerAndReportsNormalOnce) {
    start_server();
    connect(client);
    ASSERT_EQ(server.player_count(), 1U);

    client.disconnect();
    client.disconnect();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::normal}));
    EXPECT_TRUE(
        wait_until([this] { server.receive(); }, [this] { return server.player_count() == 0; }));
}

TEST(NetworkClient, GivesUpAndReportsTimeoutWhenNoServerAnswers) {
    ManualClock clock;
    EventBus bus;
    std::vector<DisconnectReason> reasons;
    bus.subscribe<Disconnected>(
        [&reasons](const Disconnected& event) { reasons.push_back(event.reason); });
    FakeServer silent;
    NetworkClient client(clock, bus);
    std::error_code ec;
    ASSERT_TRUE(client.connect("127.0.0.1", silent.port(), ec)) << ec.message();

    for (int attempt = 0; attempt < 6; ++attempt) {
        client.update();
        clock.advance(500ms);
    }
    client.update();

    EXPECT_EQ(client.state(), ConnectionState::gave_up);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::timeout}));
}

TEST(NetworkClient, ReportsTimeoutWhenTheServerGoesSilent) {
    ManualClock clock;
    EventBus bus;
    std::vector<DisconnectReason> reasons;
    bus.subscribe<Disconnected>(
        [&reasons](const Disconnected& event) { reasons.push_back(event.reason); });
    FakeServer fake;
    NetworkClient client(clock, bus);
    std::error_code ec;
    ASSERT_TRUE(client.connect("127.0.0.1", fake.port(), ec)) << ec.message();
    client.update();
    const auto request = fake.receive();
    ASSERT_TRUE(request.has_value());
    const auto nonce = std::get<ConnectRequestPayload>(request->message.message).client_nonce;
    fake.send(request->from,
              ConnectAcceptPayload{.client_nonce = nonce, .player_entity_id = 1, .types = {}});
    ASSERT_TRUE(wait_until([&client] { client.update(); },
                           [&client] { return client.state() == ConnectionState::connected; }));

    clock.advance(5s);
    client.update();
    EXPECT_EQ(client.state(), ConnectionState::connected);
    clock.advance(1ns);
    client.update();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::timeout}));
}
