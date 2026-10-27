#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
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
#include "engine/network/StringTable.hpp"

#include "PayloadSamples.hpp"
#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::core::time::SteadyClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectionState;
using engine::network::ConnectRequestPayload;
using engine::network::Disconnected;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EntityDestroyed;
using engine::network::EntitySpawned;
using engine::network::HeartbeatPayload;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::MusicChanged;
using engine::network::NetworkClient;
using engine::network::NetworkServer;
using engine::network::PlayerLeft;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::ShotFired;
using engine::network::SoundPlayed;
using engine::network::StringCategory;
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
        client_bus.subscribe<Disconnected>(
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

    void wait_for_client(const std::function<bool()>& done) {
        ASSERT_TRUE(wait_until([this] { client.update(); }, done));
    }

    SteadyClock clock;
    EventBus server_bus;
    EventBus client_bus;
    std::vector<DisconnectReason> reasons;
    NetworkServer server{
        clock, server_bus,
        ServerOptions{.port = 0,
                      .max_players = 1,
                      .strings = payload_samples::strings_of(
                          {{.category = StringCategory::type, .id = 1, .name = "player"},
                           {.category = StringCategory::type, .id = 2, .name = "enemy"}})}};
    NetworkClient client{clock, client_bus};
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
    EXPECT_TRUE(client.strings().entries().empty());
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
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 2});
    registry.add(entity, Position{30.0F, -40.0F});
    start_server();

    connect(client);
    ASSERT_EQ(client.state(), ConnectionState::connected);
    EXPECT_EQ(client.player_id(), 1U);
    const auto strings = client.strings();
    ASSERT_EQ(strings.entries().size(), 2U);
    EXPECT_EQ(strings.id_of(StringCategory::type, "player"), 1U);
    EXPECT_EQ(strings.id_of(StringCategory::type, "enemy"), 2U);

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
    EXPECT_TRUE(reasons.empty());
}

TEST_F(NetworkClientTest, ReportsServerFullWhenRejected) {
    start_server();
    connect(client);
    NetworkClient second(clock, client_bus);

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

TEST_F(NetworkClientTest, PublishesGameEventsInTheOrderTheServerPublishedThem) {
    std::vector<std::string> received;
    client_bus.subscribe<EntitySpawned>([&received](const EntitySpawned& event) {
        received.push_back("spawn " + std::to_string(event.entity_id) + " type " +
                           std::to_string(event.type_id) + " at " +
                           std::to_string(static_cast<int>(event.x)) + "," +
                           std::to_string(static_cast<int>(event.y)));
    });
    client_bus.subscribe<SoundPlayed>([&received](const SoundPlayed& event) {
        received.push_back("sound " + std::to_string(event.sound_id));
    });
    client_bus.subscribe<ShotFired>([&received](const ShotFired& event) {
        received.push_back("shot " + std::to_string(event.projectile_entity_id));
    });
    client_bus.subscribe<EntityDestroyed>([&received](const EntityDestroyed& event) {
        received.push_back("destroy " + std::to_string(event.entity_id));
    });
    EntityManager registry;
    start_server();
    connect(client);

    server_bus.publish(EntitySpawned{.entity_id = 4, .type_id = 2, .x = 10.4F, .y = -3.6F});
    server_bus.publish(SoundPlayed{.source_entity_id = 4, .sound_id = 7});
    server_bus.publish(ShotFired{.source_entity_id = 4, .projectile_entity_id = 5});
    server_bus.publish(EntityDestroyed{.entity_id = 4});
    server.send_snapshot(registry);

    wait_for_client([&received] { return received.size() == 4; });
    EXPECT_EQ(received, (std::vector<std::string>{"spawn 4 type 2 at 10,-4", "sound 7", "shot 5",
                                                  "destroy 4"}));
}

TEST_F(NetworkClientTest, ReceivesAudioNamesAndTheMusicAlreadyPlaying) {
    NetworkServer audio(
        clock, server_bus,
        ServerOptions{.port = 0,
                      .max_players = 1,
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

    ASSERT_TRUE(client.connect("127.0.0.1", audio.port(), ec)) << ec.message();
    ASSERT_TRUE(wait_until(
        [&] {
            audio.receive();
            client.update();
        },
        [&] { return client.state() != ConnectionState::connecting; }));

    ASSERT_EQ(client.state(), ConnectionState::connected);
    const auto strings = client.strings();
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

    wait_for_client([&musics] { return !musics.empty(); });
    EXPECT_EQ(musics, (std::vector<std::uint16_t>{engine::network::no_music}));
}

TEST_F(NetworkClientTest, PublishesPlayerLeftWhenAnotherPlayerLeaves) {
    NetworkServer two_players(clock, server_bus, ServerOptions{.port = 0, .max_players = 2});
    std::error_code ec;
    ASSERT_TRUE(two_players.start(ec)) << ec.message();
    EventBus other_bus;
    NetworkClient other(clock, other_bus);
    std::vector<std::uint32_t> left;
    client_bus.subscribe<PlayerLeft>(
        [&left](const PlayerLeft& event) { left.push_back(event.player_id); });
    const auto join = [&](NetworkClient& joining) {
        ASSERT_TRUE(joining.connect("127.0.0.1", two_players.port(), ec)) << ec.message();
        ASSERT_TRUE(wait_until(
            [&] {
                two_players.receive();
                joining.update();
            },
            [&] { return joining.state() == ConnectionState::connected; }));
    };
    join(client);
    join(other);

    other.disconnect();

    ASSERT_TRUE(wait_until(
        [&] {
            two_players.receive();
            client.update();
        },
        [&left] { return !left.empty(); }));
    EXPECT_EQ(left, (std::vector<std::uint32_t>{2}));
}

TEST_F(NetworkClientTest, DropsASnapshotWithATypeMissingFromTheTable) {
    std::vector<std::uint32_t> spawned;
    client_bus.subscribe<EntitySpawned>(
        [&spawned](const EntitySpawned& event) { spawned.push_back(event.entity_id); });
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
    server_bus.publish(EntitySpawned{.entity_id = 3, .type_id = 1, .x = 0.0F, .y = 0.0F});
    server.send_snapshot(registry);

    wait_for_client([&spawned] { return !spawned.empty(); });
    EXPECT_NE(client.server_tick(), std::optional<std::uint32_t>{1U});
    wait_for_client([this] { return client.server_tick() == 2U; });
    ASSERT_EQ(client.entities().size(), 1U);
    EXPECT_EQ(client.entities().front().type_id, 1);
}

TEST_F(NetworkClientTest, DropsASpawnEventWithATypeMissingFromTheTable) {
    std::vector<std::uint16_t> spawned;
    client_bus.subscribe<EntitySpawned>(
        [&spawned](const EntitySpawned& event) { spawned.push_back(event.type_id); });
    EntityManager registry;
    start_server();
    connect(client);

    server_bus.publish(EntitySpawned{.entity_id = 3, .type_id = 9, .x = 0.0F, .y = 0.0F});
    server_bus.publish(EntitySpawned{.entity_id = 4, .type_id = 1, .x = 0.0F, .y = 0.0F});
    server.send_snapshot(registry);

    wait_for_client([this] { return client.server_tick().has_value(); });
    EXPECT_EQ(spawned, (std::vector<std::uint16_t>{1}));
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

TEST(NetworkClient, MeasuresTheRoundTripToTheServer) {
    ManualClock clock;
    EventBus bus;
    FakeServer fake;
    NetworkClient client(clock, bus);
    EXPECT_FALSE(client.stats().round_trip_time.has_value());
    std::error_code ec;
    ASSERT_TRUE(client.connect("127.0.0.1", fake.port(), ec)) << ec.message();
    client.update();
    const auto request = fake.receive();
    ASSERT_TRUE(request.has_value());
    const auto nonce = std::get<ConnectRequestPayload>(request->message.message).client_nonce;
    fake.send(request->from, ConnectAcceptPayload{.client_nonce = nonce, .player_entity_id = 1});
    ASSERT_TRUE(wait_until([&client] { client.update(); },
                           [&client] { return client.state() == ConnectionState::connected; }));

    clock.advance(1s);
    client.update();
    const auto heartbeat = fake.receive();
    ASSERT_TRUE(heartbeat.has_value());
    EXPECT_TRUE(std::holds_alternative<HeartbeatPayload>(heartbeat->message.message));
    clock.advance(20ms);
    fake.send(heartbeat->from, HeartbeatPayload{});

    ASSERT_TRUE(wait_until([&client] { client.update(); },
                           [&client] { return client.stats().round_trip_time.has_value(); }));
    EXPECT_EQ(client.stats().round_trip_time, std::chrono::nanoseconds{20ms});
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
    fake.send(request->from, ConnectAcceptPayload{.client_nonce = nonce, .player_entity_id = 1});
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
