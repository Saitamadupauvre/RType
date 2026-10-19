#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <system_error>
#include <thread>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"

#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/SocketError.hpp"
#include "socket/UdpSocket.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::event::EventBus;
using engine::core::time::SteadyClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::Endpoint;
using engine::network::InputPayload;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::NetworkServer;
using engine::network::PlayerJoined;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::SnapshotPayload;
using engine::network::SocketError;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

constexpr auto delivery_deadline = 2s;

Endpoint loopback(std::uint16_t port) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

UdpSocket bound_peer() {
    UdpSocket socket(AF_INET);
    std::error_code ec;
    socket.bind(loopback(0), ec);
    EXPECT_FALSE(ec) << ec.message();
    return socket;
}

void send_to_server(UdpSocket& peer, const Message& message, std::uint32_t sequence,
                    std::uint16_t port) {
    const ProtocolHandler handler;
    const auto bytes = handler.encode(message, sequence);
    ASSERT_TRUE(bytes.has_value());
    std::error_code ec;
    peer.send_to(*bytes, loopback(port), ec);
    ASSERT_FALSE(ec) << ec.message();
}

std::optional<ReceivedMessage> receive_from_server(UdpSocket& peer) {
    std::array<std::uint8_t, max_datagram_size> buffer{};
    Endpoint from;
    std::error_code ec;
    const auto size = peer.receive_from(buffer, from, delivery_deadline, ec);
    if (ec) {
        return std::nullopt;
    }
    const ProtocolHandler handler;
    return handler.decode(std::span(buffer).first(size));
}

bool receive_until(NetworkServer& server, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        server.receive();
        std::this_thread::yield();
    }
    return true;
}

class NetworkServerTest : public testing::Test {
protected:
    void start() {
        std::error_code ec;
        ASSERT_TRUE(server.start(ec)) << ec.message();
    }

    SteadyClock clock;
    EventBus bus;
    NetworkServer server{clock, bus, ServerOptions{.port = 0, .max_players = 2}};
};

} // namespace

TEST_F(NetworkServerTest, StartsOnAFreePortAndStops) {
    start();

    EXPECT_NE(server.port(), 0);
    EXPECT_EQ(server.player_count(), 0U);
    server.receive();
    server.stop();
    server.stop();
}

TEST_F(NetworkServerTest, ReportsTheRequestedPortBeforeStarting) {
    NetworkServer unstarted(clock, bus, ServerOptions{.port = 4242, .max_players = 1});

    EXPECT_EQ(unstarted.port(), 4242);
}

TEST_F(NetworkServerTest, StartReportsPortAlreadyInUse) {
    start();
    NetworkServer second(clock, bus, ServerOptions{.port = server.port(), .max_players = 1});
    std::error_code ec;

    EXPECT_FALSE(second.start(ec));
    EXPECT_EQ(ec, SocketError::address_in_use);
}

TEST_F(NetworkServerTest, ServesAClientOverLoopback) {
    std::vector<std::uint32_t> joined;
    bus.subscribe<PlayerJoined>(
        [&joined](const PlayerJoined& event) { joined.push_back(event.player_id); });
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 1});
    registry.add(entity, Position{3.0F, 4.0F});
    start();
    auto peer = bound_peer();

    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, server.port());
    ASSERT_TRUE(receive_until(server, [this] { return server.player_count() == 1; }));
    const auto accept = receive_from_server(peer);
    ASSERT_TRUE(accept.has_value());
    EXPECT_EQ(std::get<ConnectAcceptPayload>(accept->message).client_nonce, 7U);
    EXPECT_EQ(joined, (std::vector<std::uint32_t>{1}));

    send_to_server(peer, InputPayload{.action_flags = 1, .move_x = -1, .move_y = 1}, 1,
                   server.port());
    ASSERT_TRUE(receive_until(server, [this] { return server.inputs().front().move_x == -1; }));
    EXPECT_EQ(server.inputs().front().player_id, 1U);
    EXPECT_EQ(server.inputs().front().move_y, 1);

    server.send_snapshot(registry);
    const auto snapshot = receive_from_server(peer);
    ASSERT_TRUE(snapshot.has_value());
    const auto& payload = std::get<SnapshotPayload>(snapshot->message);
    EXPECT_EQ(payload.server_tick, 1U);
    ASSERT_EQ(payload.entities.size(), 1U);
    EXPECT_EQ(payload.entities.front().y, 4);
}
