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
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"

#include "PayloadSamples.hpp"
#include "TestFields.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/SocketError.hpp"
#include "socket/UdpSocket.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::event::EventBus;
using engine::core::time::SteadyClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EventPayload;
using engine::network::InputPayload;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::MusicChanged;
using engine::network::MusicEvent;
using engine::network::NamedEvent;
using engine::network::NamedEventState;
using engine::network::NetworkServer;
using engine::network::PlayerJoined;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::SnapshotPayload;
using engine::network::SocketError;
using engine::network::StringCategory;
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

    static engine::core::FieldRegistry core_fields() {
        engine::core::FieldRegistry fields;
        engine::core::register_core_fields(fields);
        return fields;
    }

    SteadyClock clock;
    EventBus bus;
    const engine::core::FieldRegistry fields = core_fields();
    const engine::core::FieldRegistry no_fields;
    NetworkServer server{clock, bus, fields, ServerOptions{.port = 0, .max_players = 2}};
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
    NetworkServer unstarted(clock, bus, fields, ServerOptions{.port = 4242, .max_players = 1});

    EXPECT_EQ(unstarted.port(), 4242);
}

TEST_F(NetworkServerTest, StartReportsPortAlreadyInUse) {
    start();
    NetworkServer second(clock, bus, fields,
                         ServerOptions{.port = server.port(), .max_players = 1});
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
    const auto pos_id = std::get<ConnectAcceptPayload>(accept->message)
                            .strings.id_of(StringCategory::field, "Position.pos");
    ASSERT_TRUE(pos_id.has_value());
    EXPECT_EQ(payload.entities.front().fields,
              (std::vector<engine::network::FieldState>{
                  {.field_id = *pos_id, .value = engine::core::Vec2{3.F, 4.F}}}));
}

TEST_F(NetworkServerTest, StartRejectsAReplicatedListField) {
    engine::core::FieldRegistry listed;
    listed.add<engine::core::Tags>(
        "Tags", {engine::core::list_field<engine::core::Tags>(
                    "labels", engine::core::FieldKind::String,
                    engine::core::field_flags::replicated, engine::core::labels_to_list,
                    [](engine::core::Tags& tags, const engine::core::FieldList& list) {
                        tags = engine::core::labels_from_list(list);
                    })});
    NetworkServer refusing(clock, bus, listed, ServerOptions{.port = 0, .max_players = 1});
    std::error_code ec;

    EXPECT_FALSE(refusing.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, StartRejectsReplicatedFieldNamesTooLargeForOnePacket) {
    const auto many_fields = test_fields::registry();
    NetworkServer crowded(clock, bus, many_fields,
                          ServerOptions{.port = 0,
                                        .max_players = 1,
                                        .strings = payload_samples::long_name_strings(17)});
    NetworkServer roomy(clock, bus, no_fields,
                        ServerOptions{.port = 0,
                                      .max_players = 1,
                                      .strings = payload_samples::long_name_strings(17)});
    std::error_code ec;

    EXPECT_FALSE(crowded.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_TRUE(roomy.start(ec)) << ec.message();
}

TEST_F(NetworkServerTest, StartRejectsAStringTableTooLargeForOnePacket) {
    NetworkServer crowded(clock, bus, no_fields,
                          ServerOptions{.port = 0,
                                        .max_players = 1,
                                        .strings = payload_samples::long_name_strings(18)});
    std::error_code ec;

    EXPECT_FALSE(crowded.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, SendsTheStringTableAndTheCurrentMusicToPlayersJoiningLater) {
    const auto strings = payload_samples::strings_of(
        {{.category = StringCategory::sound, .id = 1, .name = "shot.wav"},
         {.category = StringCategory::music, .id = 2, .name = "stage.ogg"}});
    NetworkServer audio(clock, bus, no_fields,
                        ServerOptions{.port = 0, .max_players = 1, .strings = strings});
    std::error_code ec;
    ASSERT_TRUE(audio.start(ec)) << ec.message();
    bus.publish(MusicChanged{.music_id = 2});
    auto peer = bound_peer();

    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, audio.port());
    ASSERT_TRUE(receive_until(audio, [&audio] { return audio.player_count() == 1; }));
    const auto accept = receive_from_server(peer);

    ASSERT_TRUE(accept.has_value());
    const auto& payload = std::get<ConnectAcceptPayload>(accept->message);
    EXPECT_EQ(payload.strings, strings);
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

TEST_F(NetworkServerTest, SendsItsStringTableAndPublishedEventsToPlayers) {
    const auto strings = payload_samples::strings_of(
        {{.category = StringCategory::type, .id = 3, .name = "bydo"},
         {.category = StringCategory::event, .id = 2, .name = "boom"},
         {.category = StringCategory::event_key, .id = 5, .name = "at"}});
    NetworkServer typed(clock, bus, no_fields,
                        ServerOptions{.port = 0, .max_players = 1, .strings = strings});
    std::error_code ec;
    ASSERT_TRUE(typed.start(ec)) << ec.message();
    auto peer = bound_peer();
    send_to_server(peer, ConnectRequestPayload{.client_nonce = 7}, 0, typed.port());
    ASSERT_TRUE(receive_until(typed, [&typed] { return typed.player_count() == 1; }));
    const auto accept = receive_from_server(peer);
    ASSERT_TRUE(accept.has_value());
    EXPECT_EQ(std::get<ConnectAcceptPayload>(accept->message).strings, strings);
    EntityManager registry;

    bus.publish(NamedEvent{.name = "missing", .arguments = {}});
    bus.publish(
        NamedEvent{.name = "boom", .arguments = {{.key = "at", .value = Vec2{1.0F, 2.0F}}}});
    typed.send_snapshot(registry);

    const auto event = receive_from_server(peer);
    ASSERT_TRUE(event.has_value());
    const NamedEventState boom{.name_id = 2,
                               .arguments = {{.key_id = 5, .value = Vec2{1.0F, 2.0F}}}};
    EXPECT_EQ(event->message, Message{EventPayload{boom}});
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
