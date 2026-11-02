#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <system_error>
#include <thread>
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
#include "engine/network/Transport.hpp"

#include "PayloadSamples.hpp"
#include "socket/SocketError.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Key;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ClientTransport;
using engine::network::ConnectionState;
using engine::network::Disconnected;
using engine::network::DisconnectReason;
using engine::network::LoopbackNetwork;
using engine::network::NamedEvent;
using engine::network::NetworkClient;
using engine::network::NetworkServer;
using engine::network::PlayerJoined;
using engine::network::PlayerLeft;
using engine::network::Replicated;
using engine::network::ServerOptions;
using engine::network::ServerTransport;
using engine::network::SocketError;
using engine::network::StringCategory;
using engine::network::UdpClientTransport;
using engine::network::UdpServerTransport;
using namespace std::chrono_literals;

constexpr auto delivery_deadline = 2s;
constexpr auto session_timeout = 5s;

const engine::core::FieldRegistry& known_fields() {
    static const engine::core::FieldRegistry registry = [] {
        engine::core::FieldRegistry fields;
        engine::core::register_core_fields(fields);
        return fields;
    }();
    return registry;
}

ServerOptions options(std::size_t max_players) {
    return ServerOptions{.max_players = max_players,
                         .strings = payload_samples::strings_of(
                             {{.category = StringCategory::type, .id = 1, .name = "ship"},
                              {.category = StringCategory::event, .id = 1, .name = "boom"}})};
}

bool pump_until(const std::function<void()>& step, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    step();
    while (!done()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
        step();
    }
    return true;
}

struct Player {
    Player(ManualClock& clock, ClientTransport& transport)
        : client(clock, events, known_fields(), transport) {
        events.subscribe<Disconnected>(
            [this](const Disconnected& event) { reasons.push_back(event.reason); });
        events.subscribe<NamedEvent>(
            [this](const NamedEvent& event) { received.push_back(event.name); });
        events.subscribe<PlayerLeft>(
            [this](const PlayerLeft& event) { left.push_back(event.player_id); });
    }

    EventBus events;
    NetworkClient client;
    std::vector<DisconnectReason> reasons;
    std::vector<std::string> received;
    std::vector<std::uint32_t> left;
};

class Rig {
public:
    Rig() = default;
    virtual ~Rig() = default;
    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;
    Rig(Rig&&) = delete;
    Rig& operator=(Rig&&) = delete;

    [[nodiscard]] virtual ServerTransport& server() = 0;
    [[nodiscard]] virtual ClientTransport& add_client() = 0;
};

class LoopbackRig final : public Rig {
public:
    [[nodiscard]] ServerTransport& server() override { return _network.server(); }
    [[nodiscard]] ClientTransport& add_client() override { return _network.add_client(); }

private:
    LoopbackNetwork _network;
};

class UdpRig final : public Rig {
public:
    [[nodiscard]] ServerTransport& server() override { return _server; }
    [[nodiscard]] ClientTransport& add_client() override {
        return *_clients.emplace_back(
            std::make_unique<UdpClientTransport>("127.0.0.1", _server.port()));
    }

private:
    UdpServerTransport _server{0};
    std::vector<std::unique_ptr<UdpClientTransport>> _clients;
};

enum class TransportKind : std::uint8_t { loopback, udp };

std::string transport_name(const testing::TestParamInfo<TransportKind>& info) {
    return info.param == TransportKind::udp ? "Udp" : "Loopback";
}

std::unique_ptr<Rig> make_rig(TransportKind kind) {
    if (kind == TransportKind::udp) {
        return std::make_unique<UdpRig>();
    }
    return std::make_unique<LoopbackRig>();
}

class EveryTransport : public testing::TestWithParam<TransportKind> {
protected:
    EveryTransport() {
        server_bus.subscribe<PlayerLeft>(
            [this](const PlayerLeft& event) { server_left.push_back(event.player_id); });
    }

    void SetUp() override {
        std::error_code ec;
        ASSERT_TRUE(server.start(ec)) << ec.message();
    }

    Player& add_player() {
        return *players.emplace_back(std::make_unique<Player>(clock, rig->add_client()));
    }

    void connect(Player& player) {
        std::error_code ec;
        ASSERT_TRUE(player.client.connect(ec)) << ec.message();
        ASSERT_TRUE(pump_until(
            [&] {
                player.client.update();
                server.receive();
            },
            [&] { return player.client.state() != ConnectionState::connecting; }));
    }

    ManualClock clock;
    EventBus server_bus;
    std::unique_ptr<Rig> rig = make_rig(GetParam());
    NetworkServer server{clock, server_bus, known_fields(), rig->server(), options(2)};
    std::vector<std::unique_ptr<Player>> players;
    std::vector<std::uint32_t> server_left;
};

} // namespace

INSTANTIATE_TEST_SUITE_P(Transports, EveryTransport,
                         testing::Values(TransportKind::loopback, TransportKind::udp),
                         transport_name);

TEST_P(EveryTransport, ConnectsAPlayer) {
    auto& player = add_player();

    connect(player);

    EXPECT_EQ(player.client.state(), ConnectionState::connected);
    EXPECT_EQ(player.client.player_id(), 1U);
    EXPECT_EQ(server.player_count(), 1U);
}

TEST_P(EveryTransport, DisconnectEndsTheSessionOnBothSides) {
    auto& leaving = add_player();
    auto& staying = add_player();
    connect(leaving);
    connect(staying);

    leaving.client.disconnect();

    EXPECT_EQ(leaving.reasons, (std::vector{DisconnectReason::normal}));
    ASSERT_TRUE(pump_until(
        [&] {
            server.receive();
            staying.client.update();
        },
        [&] { return !staying.left.empty(); }));
    EXPECT_EQ(server.player_count(), 1U);
    EXPECT_EQ(server_left, (std::vector<std::uint32_t>{1}));
    EXPECT_EQ(staying.left, (std::vector<std::uint32_t>{1}));
}

TEST_P(EveryTransport, ServerDropsAPlayerSilentForLongerThanTheTimeout) {
    auto& player = add_player();
    connect(player);

    clock.advance(session_timeout);
    server.receive();
    EXPECT_EQ(server.player_count(), 1U);
    clock.advance(1ns);
    server.receive();

    EXPECT_EQ(server.player_count(), 0U);
    EXPECT_EQ(server_left, (std::vector<std::uint32_t>{1}));
    ASSERT_TRUE(
        pump_until([&] { player.client.update(); }, [&] { return !player.reasons.empty(); }));
    EXPECT_EQ(player.reasons, (std::vector{DisconnectReason::timeout}));
}

TEST_P(EveryTransport, PlayerGivesUpOnAServerSilentForLongerThanTheTimeout) {
    auto& player = add_player();
    connect(player);

    clock.advance(session_timeout);
    player.client.update();
    EXPECT_EQ(player.client.state(), ConnectionState::connected);
    clock.advance(1ns);
    player.client.update();

    EXPECT_EQ(player.client.state(), ConnectionState::disconnected);
    EXPECT_EQ(player.reasons, (std::vector{DisconnectReason::timeout}));
}

TEST_P(EveryTransport, ShutdownDisconnectsEveryPlayer) {
    auto& first = add_player();
    auto& second = add_player();
    connect(first);
    connect(second);

    server.stop();

    ASSERT_TRUE(pump_until(
        [&] {
            first.client.update();
            second.client.update();
        },
        [&] { return !first.reasons.empty() && !second.reasons.empty(); }));
    EXPECT_EQ(first.reasons, (std::vector{DisconnectReason::server_shutdown}));
    EXPECT_EQ(second.reasons, (std::vector{DisconnectReason::server_shutdown}));
}

TEST(LoopbackScenario, ServerAndTwoClientsPlayAGameTogether) {
    ManualClock clock;
    EventBus server_bus;
    std::vector<std::uint32_t> joined;
    std::vector<std::uint32_t> server_left;
    server_bus.subscribe<PlayerJoined>(
        [&joined](const PlayerJoined& event) { joined.push_back(event.player_id); });
    server_bus.subscribe<PlayerLeft>(
        [&server_left](const PlayerLeft& event) { server_left.push_back(event.player_id); });
    LoopbackNetwork network;
    NetworkServer server(clock, server_bus, known_fields(), network.server(), options(2));
    std::error_code ec;
    ASSERT_TRUE(server.start(ec)) << ec.message();
    auto& first_transport = network.add_client();
    Player first(clock, first_transport);
    Player second(clock, network.add_client());
    EntityManager world;
    world.register_component<Replicated>();
    world.register_component<Position>();
    const auto ship = world.spawn();
    world.add(ship, Replicated{.type_id = 1});
    world.add(ship, Position{10.F, 20.F});

    ASSERT_TRUE(first.client.connect(ec));
    ASSERT_TRUE(second.client.connect(ec));
    first.client.update();
    second.client.update();
    server.receive();
    first.client.update();
    second.client.update();
    ASSERT_EQ(first.client.state(), ConnectionState::connected);
    ASSERT_EQ(second.client.state(), ConnectionState::connected);
    EXPECT_EQ(joined, (std::vector<std::uint32_t>{1, 2}));

    first.client.send_input(InputState{.keys = {Key::Up}, .mouse_buttons = {}, .pointer = {}});
    second.client.send_input(InputState{.keys = {Key::Down}, .mouse_buttons = {}, .pointer = {}});
    server.receive();
    const auto inputs = server.inputs();
    ASSERT_EQ(inputs.size(), 2U);
    EXPECT_EQ(inputs[0].input.keys, std::set<Key>{Key::Up});
    EXPECT_EQ(inputs[1].input.keys, std::set<Key>{Key::Down});

    server_bus.publish(NamedEvent{.name = "boom", .arguments = {}});
    server.send_snapshot(world);
    first.client.update();
    second.client.update();
    for (const Player* player : {&first, &second}) {
        EXPECT_EQ(player->received, (std::vector<std::string>{"boom"}));
        ASSERT_EQ(player->client.entities().size(), 1U);
        const engine::core::FieldValue expected = Vec2{10.F, 20.F};
        EXPECT_EQ(*player->client.entities().front().find("Position", "pos"), expected);
    }

    network.cut(first_transport);
    clock.advance(session_timeout + 1ns);
    second.client.send_input(InputState{});
    server.receive();
    EXPECT_EQ(server.player_count(), 1U);
    EXPECT_EQ(server_left, (std::vector<std::uint32_t>{1}));
    first.client.update();
    EXPECT_EQ(first.reasons, (std::vector{DisconnectReason::timeout}));

    second.client.update();
    EXPECT_EQ(second.left, (std::vector<std::uint32_t>{1}));
    second.client.disconnect();
    server.receive();
    EXPECT_EQ(server.player_count(), 0U);
    EXPECT_EQ(server_left, (std::vector<std::uint32_t>{1, 2}));
}

TEST(UdpServerTransport, ReportsTheRequestedPortUntilOpened) {
    const UdpServerTransport transport(4242);

    EXPECT_EQ(transport.port(), 4242);
}

TEST(UdpServerTransport, OpensOnAFreePort) {
    ManualClock clock;
    EventBus bus;
    UdpServerTransport transport(0);
    NetworkServer server(clock, bus, known_fields(), transport, options(1));
    std::error_code ec;

    ASSERT_TRUE(server.start(ec)) << ec.message();

    EXPECT_NE(transport.port(), 0);
    server.stop();
    server.stop();
}

TEST(UdpServerTransport, StartReportsAPortAlreadyInUse) {
    ManualClock clock;
    EventBus bus;
    UdpServerTransport first_transport(0);
    NetworkServer first(clock, bus, known_fields(), first_transport, options(1));
    std::error_code ec;
    ASSERT_TRUE(first.start(ec)) << ec.message();
    UdpServerTransport second_transport(first_transport.port());
    NetworkServer second(clock, bus, known_fields(), second_transport, options(1));

    EXPECT_FALSE(second.start(ec));

    EXPECT_EQ(ec, SocketError::address_in_use);
}
