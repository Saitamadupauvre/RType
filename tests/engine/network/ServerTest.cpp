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
using engine::network::AudioName;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DestroyEvent;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EntityDestroyed;
using engine::network::EventPayload;
using engine::network::InputPayload;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::MusicChanged;
using engine::network::MusicEvent;
using engine::network::NetworkServer;
using engine::network::PlayerJoined;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::Replicated;
using engine::network::ReplicatedType;
using engine::network::ServerOptions;
using engine::network::SnapshotPayload;
using engine::network::SocketError;
using engine::network::TypeEntry;
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

TEST_F(NetworkServerTest, StartRejectsAnInvalidTypeTable) {
    NetworkServer duplicated(clock, bus,
                             ServerOptions{.port = 0,
                                           .max_players = 1,
                                           .types = {ReplicatedType{.type_id = 1, .name = "a"},
                                                     ReplicatedType{.type_id = 1, .name = "b"}}});
    NetworkServer unnamed(clock, bus,
                          ServerOptions{.port = 0,
                                        .max_players = 1,
                                        .types = {ReplicatedType{.type_id = 1, .name = ""}}});
    std::error_code ec;

    EXPECT_FALSE(duplicated.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_FALSE(unnamed.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, StartRejectsInvalidSoundOrMusicTables) {
    NetworkServer duplicated_sound(clock, bus,
                                   ServerOptions{.port = 0,
                                                 .max_players = 1,
                                                 .sounds = {AudioName{.id = 1, .name = "a.wav"},
                                                            AudioName{.id = 2, .name = "a.wav"}}});
    NetworkServer reserved_music(
        clock, bus,
        ServerOptions{.port = 0,
                      .max_players = 1,
                      .musics = {AudioName{.id = engine::network::no_music, .name = "a.ogg"}}});
    std::error_code ec;

    EXPECT_FALSE(duplicated_sound.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_FALSE(reserved_music.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, SendsAudioTablesAndTheCurrentMusicToPlayersJoiningLater) {
    NetworkServer audio(clock, bus,
                        ServerOptions{.port = 0,
                                      .max_players = 1,
                                      .sounds = {AudioName{.id = 1, .name = "shot.wav"}},
                                      .musics = {AudioName{.id = 2, .name = "stage.ogg"}}});
    std::error_code ec;
    ASSERT_TRUE(audio.start(ec)) << ec.message();
    bus.publish(MusicChanged{.music_id = 2});
    auto peer = bound_peer();

    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, audio.port());
    ASSERT_TRUE(receive_until(audio, [&audio] { return audio.player_count() == 1; }));
    const auto accept = receive_from_server(peer);

    ASSERT_TRUE(accept.has_value());
    const auto& payload = std::get<ConnectAcceptPayload>(accept->message);
    EXPECT_EQ(payload.sounds, (std::vector<TypeEntry>{{.type_id = 1, .name = "shot.wav"}}));
    EXPECT_EQ(payload.musics, (std::vector<TypeEntry>{{.type_id = 2, .name = "stage.ogg"}}));
    EXPECT_EQ(payload.current_music, 2);
}

TEST_F(NetworkServerTest, SendsMusicChangesToPlayers) {
    start();
    auto peer = bound_peer();
    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, server.port());
    ASSERT_TRUE(receive_until(server, [this] { return server.player_count() == 1; }));
    ASSERT_TRUE(receive_from_server(peer).has_value());
    EntityManager registry;

    bus.publish(MusicChanged{.music_id = engine::network::no_music});
    server.send_snapshot(registry);

    const auto event = receive_from_server(peer);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->message,
              Message{EventPayload{MusicEvent{.music_id = engine::network::no_music}}});
}

TEST_F(NetworkServerTest, SendsItsTypeTableAndPublishedEventsToPlayers) {
    NetworkServer typed(clock, bus,
                        ServerOptions{.port = 0,
                                      .max_players = 1,
                                      .types = {ReplicatedType{.type_id = 3, .name = "bydo"}}});
    std::error_code ec;
    ASSERT_TRUE(typed.start(ec)) << ec.message();
    auto peer = bound_peer();
    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, typed.port());
    ASSERT_TRUE(receive_until(typed, [&typed] { return typed.player_count() == 1; }));
    const auto accept = receive_from_server(peer);
    ASSERT_TRUE(accept.has_value());
    EXPECT_EQ(std::get<ConnectAcceptPayload>(accept->message).types,
              (std::vector<TypeEntry>{{.type_id = 3, .name = "bydo"}}));
    EntityManager registry;

    bus.publish(EntityDestroyed{.entity_id = 12});
    typed.send_snapshot(registry);

    const auto event = receive_from_server(peer);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->message, Message{EventPayload{DestroyEvent{.entity_id = 12}}});
    const auto snapshot = receive_from_server(peer);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_TRUE(std::holds_alternative<SnapshotPayload>(snapshot->message));
}

TEST_F(NetworkServerTest, ReportsStatsForEachConnectedPlayer) {
    start();
    EXPECT_TRUE(server.stats().empty());
    auto peer = bound_peer();

    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, server.port());
    ASSERT_TRUE(receive_until(server, [this] { return server.player_count() == 1; }));

    const auto stats = server.stats();
    ASSERT_EQ(stats.size(), 1U);
    EXPECT_EQ(stats.front().player_id, 1U);
    EXPECT_EQ(stats.front().network.packets_lost, 0U);
}

TEST_F(NetworkServerTest, StopTellsConnectedPlayersTheServerIsShuttingDown) {
    start();
    auto peer = bound_peer();
    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, server.port());
    ASSERT_TRUE(receive_until(server, [this] { return server.player_count() == 1; }));
    ASSERT_TRUE(receive_from_server(peer).has_value());

    server.stop();

    const auto goodbye = receive_from_server(peer);
    ASSERT_TRUE(goodbye.has_value());
    EXPECT_EQ(goodbye->message,
              Message{DisconnectPayload{.reason = DisconnectReason::server_shutdown}});
    EXPECT_EQ(server.player_count(), 0U);
}
