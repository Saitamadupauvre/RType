#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/network/Transport.hpp"

#include "LoopbackPeers.hpp"
#include "PayloadSamples.hpp"
#include "protocol/Messages.hpp"
#include "transport/LoopbackLinks.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Key;
using engine::core::MouseButton;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ClientTransport;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectionState;
using engine::network::ConnectRequestPayload;
using engine::network::Disconnected;
using engine::network::DisconnectReason;
using engine::network::HeartbeatPayload;
using engine::network::Inbound;
using engine::network::LoopbackHub;
using engine::network::LoopbackLink;
using engine::network::LoopbackNetwork;
using engine::network::Message;
using engine::network::MusicChanged;
using engine::network::NamedEvent;
using engine::network::NetworkClient;
using engine::network::NetworkServer;
using engine::network::PeerId;
using engine::network::PlayerLeft;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::SoundPlayed;
using engine::network::StringCategory;
using engine::network::UdpClientTransport;
using loopback_peers::HubClientTransport;
using namespace std::chrono_literals;

const engine::core::FieldRegistry& known_fields() {
    static const engine::core::FieldRegistry registry = [] {
        engine::core::FieldRegistry fields;
        engine::core::register_core_fields(fields);
        return fields;
    }();
    return registry;
}

void join(NetworkServer& server, NetworkClient& joining) {
    std::error_code ec;
    ASSERT_TRUE(joining.connect(ec)) << ec.message();
    joining.update();
    server.receive();
    joining.update();
}

class FakeServer {
public:
    FakeServer() { loopback_peers::open(_hub.server()); }

    [[nodiscard]] ClientTransport& transport() noexcept { return _transport; }

    std::optional<Inbound> receive() {
        auto inbound = _hub.server().poll();
        if (inbound.size() != 1) {
            return std::nullopt;
        }
        return std::move(inbound.front());
    }

    void send(PeerId to, Message message) {
        loopback_peers::send(_hub.server(), to, std::move(message));
    }

private:
    LoopbackHub _hub;
    HubClientTransport _transport{_hub.add_client()};
};

void accepting(FakeServer& fake, NetworkClient& client) {
    std::error_code ec;
    EXPECT_TRUE(client.connect(ec)) << ec.message();
    client.update();
    const auto request = fake.receive();
    EXPECT_TRUE(request.has_value());
    if (request) {
        const auto nonce = std::get<ConnectRequestPayload>(request->message.message).client_nonce;
        fake.send(request->from,
                  ConnectAcceptPayload{.client_nonce = nonce, .player_entity_id = 1});
    }
    client.update();
    EXPECT_EQ(client.state(), ConnectionState::connected);
}

class NetworkClientTest : public testing::Test {
protected:
    NetworkClientTest() {
        client_bus.subscribe<Disconnected>(
            [this](const Disconnected& event) { reasons.push_back(event.reason); });
    }

    void start_server() {
        std::error_code ec;
        ASSERT_TRUE(server.start(ec)) << ec.message();
    }

    void connect(NetworkClient& joining) { join(server, joining); }

    ManualClock clock;
    EventBus server_bus;
    EventBus client_bus;
    std::vector<DisconnectReason> reasons;
    LoopbackNetwork network;
    NetworkServer server{
        clock, server_bus, known_fields(), network.server(),
        ServerOptions{.max_players = 1,
                      .strings = payload_samples::strings_of(
                          {{.category = StringCategory::type, .id = 1, .name = "player"},
                           {.category = StringCategory::type, .id = 2, .name = "enemy"},
                           {.category = StringCategory::event, .id = 1, .name = "explosion"},
                           {.category = StringCategory::event_key, .id = 1, .name = "at"}})}};
    NetworkClient client{clock, client_bus, known_fields(), network.add_client()};
};

} // namespace

TEST_F(NetworkClientTest, IsDisconnectedAndInertBeforeConnecting) {
    client.update();
    client.send_input(InputState{.keys = {Key::Space}, .mouse_buttons = {}, .pointer = {}});
    client.disconnect();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(client.player_id(), 0U);
    EXPECT_FALSE(client.server_tick().has_value());
    EXPECT_TRUE(client.entities().empty());
    EXPECT_TRUE(client.strings().entries().empty());
    EXPECT_TRUE(reasons.empty());
}

TEST_F(NetworkClientTest, ConnectRejectsANonNumericUdpAddress) {
    UdpClientTransport nowhere("not an address", 4242);
    NetworkClient udp(clock, client_bus, known_fields(), nowhere);
    std::error_code ec;

    EXPECT_FALSE(udp.connect(ec));

    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_EQ(udp.state(), ConnectionState::disconnected);
}

TEST_F(NetworkClientTest, ConnectsOnlyOnce) {
    std::error_code ec;
    ASSERT_TRUE(client.connect(ec)) << ec.message();

    EXPECT_FALSE(client.connect(ec));

    EXPECT_EQ(ec, std::errc::already_connected);
}

TEST_F(NetworkClientTest, ReceivesTheWorldAndSendsInputs) {
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 2});
    registry.add(entity, Position{30.0F, -40.0F});
    start_server();

    connect(client);
    ASSERT_EQ(client.state(), ConnectionState::connected);
    EXPECT_EQ(client.player_id(), 1U);
    const auto strings = client.strings();
    ASSERT_EQ(strings.entries(StringCategory::type).size(), 2U);
    EXPECT_EQ(strings.id_of(StringCategory::type, "player"), 1U);
    EXPECT_EQ(strings.id_of(StringCategory::type, "enemy"), 2U);

    const InputState held{.keys = {Key::Left, Key::Space},
                          .mouse_buttons = {MouseButton::Right},
                          .pointer = engine::core::Vec2{12.4F, -3.6F}};
    client.send_input(held);
    server.receive();
    ASSERT_EQ(server.inputs().size(), 1U);
    const auto sent = server.inputs().front().input;
    EXPECT_EQ(sent.keys, held.keys);
    EXPECT_EQ(sent.mouse_buttons, held.mouse_buttons);
    EXPECT_EQ(sent.pointer, (engine::core::Vec2{12.F, -4.F}));

    server.send_snapshot(registry);
    client.update();
    ASSERT_EQ(client.entities().size(), 1U);
    EXPECT_EQ(client.server_tick(), 1U);
    const auto received = client.entities().front();
    EXPECT_EQ(received.type_id, 2);
    ASSERT_EQ(received.fields.size(), 1U);
    EXPECT_EQ(received.fields.front().field.field,
              known_fields().find_field("Position", "pos").field);
    ASSERT_NE(received.find("Position", "pos"), nullptr);
    const engine::core::FieldValue expected = engine::core::Vec2{30.0F, -40.0F};
    EXPECT_EQ(*received.find("Position", "pos"), expected);
    EXPECT_EQ(received.find("Sprite", "color"), nullptr);
    EXPECT_TRUE(reasons.empty());
}

TEST_F(NetworkClientTest, ReportsServerFullWhenRejected) {
    start_server();
    connect(client);
    NetworkClient second(clock, client_bus, known_fields(), network.add_client());

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
    client.update();
    ASSERT_EQ(client.entities().size(), 1U);

    server.stop();

    client.update();
    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(client.entities().size(), 1U);
    client.update();
    client.disconnect();
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::server_shutdown}));
}

TEST_F(NetworkClientTest, PublishesGameEventsInTheOrderTheServerPublishedThem) {
    std::vector<std::string> received;
    client_bus.subscribe<NamedEvent>([&received](const NamedEvent& event) {
        const auto* at = event.find("at");
        const auto position = at != nullptr ? std::get<Vec2>(*at) : Vec2{};
        received.push_back(event.name + " at " + std::to_string(static_cast<int>(position.x)) +
                           "," + std::to_string(static_cast<int>(position.y)));
    });
    client_bus.subscribe<SoundPlayed>([&received](const SoundPlayed& event) {
        received.push_back("sound " + std::to_string(event.sound_id));
    });
    EntityManager registry;
    start_server();
    connect(client);

    server_bus.publish(
        NamedEvent{.name = "explosion", .arguments = {{.key = "at", .value = Vec2{10.4F, -3.6F}}}});
    server_bus.publish(SoundPlayed{.source_entity_id = 4, .sound_id = 7});
    server_bus.publish(NamedEvent{.name = "explosion", .arguments = {}});
    server.send_snapshot(registry);

    client.update();
    EXPECT_EQ(received,
              (std::vector<std::string>{"explosion at 10,-4", "sound 7", "explosion at 0,0"}));
}

TEST_F(NetworkClientTest, ReceivesAudioNamesAndTheMusicAlreadyPlaying) {
    LoopbackNetwork own;
    NetworkServer audio(
        clock, server_bus, known_fields(), own.server(),
        ServerOptions{.max_players = 1,
                      .strings = payload_samples::strings_of(
                          {{.category = StringCategory::sound, .id = 1, .name = "shot.wav"},
                           {.category = StringCategory::sound, .id = 2, .name = "boom.wav"},
                           {.category = StringCategory::music, .id = 3, .name = "stage.ogg"}})});
    std::error_code ec;
    ASSERT_TRUE(audio.start(ec)) << ec.message();
    server_bus.publish(MusicChanged{.music_id = 3});
    std::vector<std::uint16_t> musics;
    client_bus.subscribe<MusicChanged>(
        [&musics](const MusicChanged& event) { musics.push_back(event.music_id); });

    NetworkClient listener(clock, client_bus, known_fields(), own.add_client());

    join(audio, listener);

    ASSERT_EQ(listener.state(), ConnectionState::connected);
    const auto strings = listener.strings();
    ASSERT_EQ(strings.entries(StringCategory::sound).size(), 2U);
    EXPECT_EQ(strings.id_of(StringCategory::sound, "shot.wav"), 1U);
    EXPECT_EQ(strings.id_of(StringCategory::sound, "boom.wav"), 2U);
    ASSERT_EQ(strings.entries(StringCategory::music).size(), 1U);
    EXPECT_EQ(strings.id_of(StringCategory::music, "stage.ogg"), 3U);
    EXPECT_EQ(musics, (std::vector<std::uint16_t>{3}));
}

TEST_F(NetworkClientTest, PublishesNoMusicOnAcceptWhenTheServerPlaysNone) {
    std::vector<std::uint16_t> musics;
    client_bus.subscribe<MusicChanged>(
        [&musics](const MusicChanged& event) { musics.push_back(event.music_id); });
    start_server();

    connect(client);

    EXPECT_EQ(client.state(), ConnectionState::connected);
    EXPECT_TRUE(musics.empty());
}

TEST_F(NetworkClientTest, PublishesMusicChangesFromTheServer) {
    std::vector<std::uint16_t> musics;
    client_bus.subscribe<MusicChanged>(
        [&musics](const MusicChanged& event) { musics.push_back(event.music_id); });
    EntityManager registry;
    start_server();
    connect(client);

    server_bus.publish(MusicChanged{.music_id = engine::network::no_music});
    server.send_snapshot(registry);

    client.update();
    EXPECT_EQ(musics, (std::vector<std::uint16_t>{engine::network::no_music}));
}

TEST_F(NetworkClientTest, PublishesPlayerLeftWhenAnotherPlayerLeaves) {
    LoopbackNetwork own;
    NetworkServer two_players(clock, server_bus, known_fields(), own.server(),
                              ServerOptions{.max_players = 2});
    std::error_code ec;
    ASSERT_TRUE(two_players.start(ec)) << ec.message();
    EventBus other_bus;
    NetworkClient first(clock, client_bus, known_fields(), own.add_client());
    NetworkClient other(clock, other_bus, known_fields(), own.add_client());
    std::vector<std::uint32_t> left;
    client_bus.subscribe<PlayerLeft>(
        [&left](const PlayerLeft& event) { left.push_back(event.player_id); });
    join(two_players, first);
    join(two_players, other);

    other.disconnect();

    two_players.receive();
    first.update();
    EXPECT_EQ(left, (std::vector<std::uint32_t>{2}));
}

TEST_F(NetworkClientTest, DropsASnapshotWithATypeMissingFromTheTable) {
    std::vector<std::string> spawned;
    client_bus.subscribe<NamedEvent>(
        [&spawned](const NamedEvent& event) { spawned.push_back(event.name); });
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    const auto unknown = registry.spawn();
    registry.add(unknown, Replicated{.type_id = 9});
    registry.add(unknown, Position{1.0F, 1.0F});
    start_server();
    connect(client);

    server.send_snapshot(registry);
    registry.get<Replicated>(unknown).type_id = 1;
    server_bus.publish(NamedEvent{.name = "explosion", .arguments = {}});
    server.send_snapshot(registry);

    client.update();
    EXPECT_EQ(spawned, (std::vector<std::string>{"explosion"}));
    EXPECT_EQ(client.server_tick(), 2U);
    ASSERT_EQ(client.entities().size(), 1U);
    EXPECT_EQ(client.entities().front().type_id, 1);
}

TEST_F(NetworkClientTest, NeverReceivesANamedEventMissingFromTheServerTable) {
    std::vector<std::string> received;
    client_bus.subscribe<NamedEvent>([&received](const NamedEvent& event) {
        received.push_back(event.name + " " + std::to_string(event.arguments.size()));
    });
    EntityManager registry;
    start_server();
    connect(client);

    server_bus.publish(NamedEvent{.name = "unknown", .arguments = {}});
    server_bus.publish(
        NamedEvent{.name = "explosion", .arguments = {{.key = "size", .value = 1.0F}}});
    server_bus.publish(NamedEvent{.name = "explosion", .arguments = {}});
    server.send_snapshot(registry);

    client.update();
    EXPECT_TRUE(client.server_tick().has_value());
    EXPECT_EQ(received, (std::vector<std::string>{"explosion 0"}));
}

TEST_F(NetworkClientTest, DisconnectTellsTheServerAndReportsNormalOnce) {
    start_server();
    connect(client);
    ASSERT_EQ(server.player_count(), 1U);

    client.disconnect();
    client.disconnect();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::normal}));
    server.receive();
    EXPECT_EQ(server.player_count(), 0U);
}

TEST(NetworkClient, GivesUpAndReportsTimeoutWhenNoServerAnswers) {
    ManualClock clock;
    EventBus bus;
    std::vector<DisconnectReason> reasons;
    bus.subscribe<Disconnected>(
        [&reasons](const Disconnected& event) { reasons.push_back(event.reason); });
    FakeServer silent;
    NetworkClient client(clock, bus, known_fields(), silent.transport());
    std::error_code ec;
    ASSERT_TRUE(client.connect(ec)) << ec.message();

    for (int attempt = 0; attempt < 6; ++attempt) {
        client.update();
        clock.advance(500ms);
    }
    client.update();

    EXPECT_EQ(client.state(), ConnectionState::gave_up);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::timeout}));
}

TEST(NetworkClient, MeasuresTheRoundTripToTheServer) {
    ManualClock clock;
    EventBus bus;
    FakeServer fake;
    NetworkClient client(clock, bus, known_fields(), fake.transport());
    EXPECT_FALSE(client.stats().round_trip_time.has_value());
    accepting(fake, client);

    clock.advance(1s);
    client.update();
    const auto heartbeat = fake.receive();
    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_TRUE(std::holds_alternative<HeartbeatPayload>(heartbeat->message.message));
    clock.advance(20ms);
    fake.send(heartbeat->from, HeartbeatPayload{});

    client.update();
    EXPECT_EQ(client.stats().round_trip_time, std::chrono::nanoseconds{20ms});
}

TEST(NetworkClient, ReportsTimeoutWhenTheServerGoesSilent) {
    ManualClock clock;
    EventBus bus;
    std::vector<DisconnectReason> reasons;
    bus.subscribe<Disconnected>(
        [&reasons](const Disconnected& event) { reasons.push_back(event.reason); });
    FakeServer fake;
    NetworkClient client(clock, bus, known_fields(), fake.transport());
    accepting(fake, client);

    clock.advance(5s);
    client.update();
    EXPECT_EQ(client.state(), ConnectionState::connected);
    clock.advance(1ns);
    client.update();

    EXPECT_EQ(client.state(), ConnectionState::disconnected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::timeout}));
}

TEST_F(NetworkClientTest, RejectsAServerReplicatingAFieldTheClientDoesNotKnow) {
    start_server();
    const engine::core::FieldRegistry no_fields;
    NetworkClient stranger(clock, client_bus, no_fields, network.add_client());

    connect(stranger);
    stranger.update();

    EXPECT_EQ(stranger.state(), ConnectionState::rejected);
    EXPECT_EQ(reasons, (std::vector{DisconnectReason::protocol_error}));
    server.receive();
    EXPECT_EQ(server.player_count(), 0U);
}
