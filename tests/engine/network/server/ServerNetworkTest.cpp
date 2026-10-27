#include <chrono>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Replicated.hpp"

#include "protocol/Messages.hpp"
#include "server/ServerNetwork.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DestroyEvent;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::Endpoint;
using engine::network::EventPayload;
using engine::network::Inbound;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::Outbound;
using engine::network::PlayerLeftEvent;
using engine::network::Replicated;
using engine::network::ServerNetwork;
using engine::network::ServerSessionsConfig;
using engine::network::SnapshotPayload;
using engine::network::SoundEvent;
using engine::network::SpawnEvent;
using namespace std::chrono_literals;

Endpoint client(std::uint16_t port) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

Inbound from(const Endpoint& endpoint, Message message, std::uint32_t sequence = 0) {
    return Inbound{.from = endpoint,
                   .message = {.header = {.sequence = sequence}, .message = std::move(message)}};
}

std::vector<const SnapshotPayload*> snapshots_to(const std::vector<Outbound>& outbound,
                                                 const Endpoint& endpoint) {
    std::vector<const SnapshotPayload*> snapshots;
    for (const auto& message : outbound) {
        if (const auto* snapshot = std::get_if<SnapshotPayload>(&message.message);
            snapshot != nullptr && message.to == endpoint) {
            snapshots.push_back(snapshot);
        }
    }
    return snapshots;
}

class ServerNetworkTest : public testing::Test {
protected:
    ServerNetworkTest() {
        registry.register_component<Replicated>();
        registry.register_component<Position>();
    }

    void spawn_replicated(float x) {
        const auto entity = registry.spawn();
        registry.add(entity, Replicated{.type_id = 1});
        registry.add(entity, Position{x, 0.0F});
    }

    void connect_clients() {
        const auto replies = network.receive({
            from(client(5000), ConnectRequestPayload{.client_nonce = 1}),
            from(client(5001), ConnectRequestPayload{.client_nonce = 2}),
        });
        ASSERT_EQ(replies.size(), 2U);
    }

    ManualClock clock;
    EventBus bus;
    EntityManager registry;
    ServerNetwork network{clock, bus, ServerSessionsConfig{.max_players = 4, .timeout = 5s}};
};

} // namespace

TEST_F(ServerNetworkTest, OneTickConnectsStoresInputsAndSendsSnapshots) {
    spawn_replicated(12.0F);

    const auto replies = network.receive({
        from(client(5000), ConnectRequestPayload{.client_nonce = 1}),
        from(client(5001), ConnectRequestPayload{.client_nonce = 2}),
        from(client(5000), InputPayload{.action_flags = 1, .move_x = 1, .move_y = 0}, 1),
    });
    const auto snapshot = network.snapshot(registry);

    ASSERT_EQ(replies.size(), 2U);
    EXPECT_TRUE(std::holds_alternative<ConnectAcceptPayload>(replies[0].message));
    EXPECT_TRUE(std::holds_alternative<ConnectAcceptPayload>(replies[1].message));
    ASSERT_EQ(network.sessions().size(), 2U);
    EXPECT_EQ(network.sessions()[0].input.move_x, 1);
    EXPECT_EQ(network.server_tick(), 1U);
    for (const auto port : {5000, 5001}) {
        const auto received = snapshots_to(snapshot, client(static_cast<std::uint16_t>(port)));
        ASSERT_EQ(received.size(), 1U) << port;
        EXPECT_EQ(received.front()->server_tick, 1U);
        ASSERT_EQ(received.front()->entities.size(), 1U);
        EXPECT_EQ(received.front()->entities.front().x, 12);
    }
}

TEST_F(ServerNetworkTest, SnapshotWithoutPlayersSendsNothingButAdvancesTick) {
    spawn_replicated(1.0F);

    EXPECT_TRUE(network.snapshot(registry).empty());
    EXPECT_TRUE(network.snapshot(registry).empty());

    EXPECT_EQ(network.server_tick(), 2U);
}

TEST_F(ServerNetworkTest, EveryPacketOfALargeWorldReachesEveryPlayer) {
    connect_clients();
    for (int index = 0; index < 100; ++index) {
        spawn_replicated(static_cast<float>(index));
    }

    const auto outbound = network.snapshot(registry);

    EXPECT_EQ(outbound.size(), 4U);
    for (const auto port : {5000, 5001}) {
        std::size_t entities = 0;
        for (const auto* snapshot :
             snapshots_to(outbound, client(static_cast<std::uint16_t>(port)))) {
            entities += snapshot->entities.size();
        }
        EXPECT_EQ(entities, 100U) << port;
    }
}

TEST_F(ServerNetworkTest, CrashedClientIsDroppedOnALaterTickAndOthersAreTold) {
    connect_clients();
    clock.advance(3s);
    static_cast<void>(network.receive({from(client(5001), InputPayload{}, 1)}));
    clock.advance(3s);

    const auto outbound = network.receive({});

    ASSERT_EQ(outbound.size(), 2U);
    EXPECT_EQ(outbound[0].to, client(5000));
    EXPECT_EQ(outbound[0].message, Message{DisconnectPayload{.reason = DisconnectReason::timeout}});
    EXPECT_EQ(outbound[1].to, client(5001));
    EXPECT_EQ(outbound[1].message, Message{EventPayload{PlayerLeftEvent{.player_entity_id = 1}}});
    ASSERT_EQ(network.sessions().size(), 1U);
}

TEST_F(ServerNetworkTest, QueuedEventsReachEveryPlayerInOrderBeforeTheSnapshot) {
    connect_clients();
    const EventPayload spawn{SpawnEvent{.entity_id = 1, .type_id = 1, .x = 2, .y = 3}};
    const EventPayload sound{SoundEvent{.source_entity_id = 1, .sound_id = 4}};
    const EventPayload destroy{DestroyEvent{.entity_id = 1}};

    network.queue_event(spawn);
    network.queue_event(sound);
    network.queue_event(destroy);
    const auto outbound = network.snapshot(registry);

    for (const auto port : {5000, 5001}) {
        std::vector<Message> received;
        for (const auto& message : outbound) {
            if (message.to == client(static_cast<std::uint16_t>(port))) {
                received.push_back(message.message);
            }
        }
        ASSERT_EQ(received.size(), 4U) << port;
        EXPECT_EQ(received[0], Message{spawn});
        EXPECT_EQ(received[1], Message{sound});
        EXPECT_EQ(received[2], Message{destroy});
        EXPECT_TRUE(std::holds_alternative<SnapshotPayload>(received[3]));
    }
}

TEST_F(ServerNetworkTest, EventsAreSentOnce) {
    connect_clients();
    network.queue_event(DestroyEvent{.entity_id = 1});
    static_cast<void>(network.snapshot(registry));

    const auto outbound = network.snapshot(registry);

    EXPECT_EQ(outbound.size(), 2U);
    EXPECT_EQ(snapshots_to(outbound, client(5000)).size(), 1U);
    EXPECT_EQ(snapshots_to(outbound, client(5001)).size(), 1U);
}

TEST_F(ServerNetworkTest, APlayerJoiningLaterOnlyGetsLaterEvents) {
    const EventPayload before{DestroyEvent{.entity_id = 1}};
    const EventPayload after{DestroyEvent{.entity_id = 2}};
    static_cast<void>(network.receive({from(client(5000), ConnectRequestPayload{})}));
    network.queue_event(before);
    static_cast<void>(network.receive({from(client(5001), ConnectRequestPayload{})}));
    network.queue_event(after);

    const auto outbound = network.snapshot(registry);

    std::vector<Message> late_player;
    for (const auto& message : outbound) {
        if (message.to == client(5001) && std::holds_alternative<EventPayload>(message.message)) {
            late_player.push_back(message.message);
        }
    }
    EXPECT_EQ(late_player, (std::vector<Message>{after}));
}

TEST_F(ServerNetworkTest, EventsWithoutPlayersAreDropped) {
    network.queue_event(DestroyEvent{.entity_id = 1});
    static_cast<void>(network.receive({from(client(5000), ConnectRequestPayload{})}));

    const auto outbound = network.snapshot(registry);

    ASSERT_EQ(outbound.size(), 1U);
    EXPECT_TRUE(std::holds_alternative<SnapshotPayload>(outbound.front().message));
}

TEST_F(ServerNetworkTest, ShutdownTellsEveryPlayerAndEndsTheirSessions) {
    connect_clients();
    const Message shutdown{DisconnectPayload{.reason = DisconnectReason::server_shutdown}};

    const auto outbound = network.shutdown();

    ASSERT_EQ(outbound.size(), 2U);
    EXPECT_EQ(outbound[0].to, client(5000));
    EXPECT_EQ(outbound[0].message, shutdown);
    EXPECT_EQ(outbound[1].to, client(5001));
    EXPECT_EQ(outbound[1].message, shutdown);
    EXPECT_TRUE(network.sessions().empty());
    EXPECT_TRUE(network.shutdown().empty());
}

TEST_F(ServerNetworkTest, MessagesFromStrangersDoNotDisturbTheTick) {
    connect_clients();

    const auto outbound = network.receive({
        from(client(6000), InputPayload{.action_flags = 1, .move_x = 1, .move_y = 1}, 1),
        from(client(6000), DisconnectPayload{.reason = DisconnectReason::normal}),
    });

    EXPECT_TRUE(outbound.empty());
    EXPECT_EQ(network.sessions().size(), 2U);
}
