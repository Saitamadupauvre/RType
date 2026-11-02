#include <cstdint>
#include <set>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"

#include "LoopbackPeers.hpp"
#include "PayloadSamples.hpp"
#include "TestFields.hpp"
#include "protocol/Messages.hpp"
#include "transport/Link.hpp"
#include "transport/LoopbackLinks.hpp"

namespace {

using engine::core::EntityManager;
using engine::core::Key;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::event::EventBus;
using engine::core::time::ManualClock;
using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::EventPayload;
using engine::network::Inbound;
using engine::network::InputPayload;
using engine::network::Link;
using engine::network::LoopbackHub;
using engine::network::LoopbackLink;
using engine::network::Message;
using engine::network::MusicChanged;
using engine::network::MusicEvent;
using engine::network::NamedEvent;
using engine::network::NamedEventState;
using engine::network::NetworkServer;
using engine::network::Outbound;
using engine::network::PeerId;
using engine::network::PlayerJoined;
using engine::network::Replicated;
using engine::network::server_peer;
using engine::network::ServerOptions;
using engine::network::SnapshotPayload;
using engine::network::StringCategory;
using engine::network::StringTable;
using loopback_peers::HubServerTransport;
using loopback_peers::messages;
using loopback_peers::open;
using loopback_peers::send;

engine::core::FieldRegistry core_fields() {
    engine::core::FieldRegistry fields;
    engine::core::register_core_fields(fields);
    return fields;
}

struct LoopbackServer {
    LoopbackServer(ManualClock& clock, EventBus& bus, const engine::core::FieldRegistry& fields,
                   ServerOptions options)
        : server(clock, bus, fields, transport, std::move(options)) {}

    bool start() {
        std::error_code ec;
        const bool started = server.start(ec);
        EXPECT_FALSE(ec) << ec.message();
        return started;
    }

    LoopbackLink& join() {
        auto& peer = open(hub.add_client());
        send(peer, server_peer, ConnectRequestPayload{.client_nonce = 7});
        server.receive();
        return peer;
    }

    LoopbackHub hub;
    HubServerTransport transport{hub.server()};
    NetworkServer server;
};

class NetworkServerTest : public testing::Test {
protected:
    ManualClock clock;
    EventBus bus;
    const engine::core::FieldRegistry fields = core_fields();
    const engine::core::FieldRegistry no_fields;
    LoopbackServer loopback{clock, bus, fields, ServerOptions{.max_players = 2}};
    NetworkServer& server = loopback.server;
};

class SpyLink final : public Link {
public:
    void open(std::error_code& ec) override { ec.clear(); }
    void close() override {}
    [[nodiscard]] std::vector<Inbound> poll() override { return std::exchange(inbox, {}); }
    void send(Outbound outbound) override { sent.push_back(std::move(outbound)); }
    void forget(PeerId peer) override { forgotten.push_back(peer); }
    [[nodiscard]] std::string describe(PeerId) const override { return "spy"; }

    std::vector<Inbound> inbox;
    std::vector<Outbound> sent;
    std::vector<PeerId> forgotten;
};

class SpyTransport final : public engine::network::ServerTransport {
public:
    SpyLink spy;

private:
    [[nodiscard]] Link& link() noexcept override { return spy; }
};

Inbound inbound(PeerId from, Message message) {
    return Inbound{.from = from, .message = {.header = {}, .message = std::move(message)}};
}

} // namespace

TEST_F(NetworkServerTest, StartsAndStopsTwice) {
    ASSERT_TRUE(loopback.start());

    EXPECT_EQ(server.player_count(), 0U);
    server.receive();
    server.stop();
    server.stop();
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
    ASSERT_TRUE(loopback.start());

    auto& peer = loopback.join();
    ASSERT_EQ(server.player_count(), 1U);
    const auto accepted = messages(peer);
    ASSERT_EQ(accepted.size(), 1U);
    const auto& accept = std::get<ConnectAcceptPayload>(accepted.front());
    EXPECT_EQ(accept.client_nonce, 7U);
    EXPECT_EQ(joined, (std::vector<std::uint32_t>{1}));

    send(peer, server_peer,
         InputPayload{.keys = {0x02},
                      .mouse_buttons = 0x01,
                      .pointer = engine::network::PointerPosition{.x = 5, .y = -6}},
         1);
    server.receive();
    const auto input = server.inputs().front();
    EXPECT_EQ(input.player_id, 1U);
    EXPECT_EQ(input.input.keys, std::set<Key>{Key::A});
    EXPECT_EQ(input.input.mouse_buttons,
              std::set<engine::core::MouseButton>{engine::core::MouseButton::Left});
    EXPECT_EQ(input.input.pointer, (engine::core::Vec2{5.F, -6.F}));

    server.send_snapshot(registry);
    const auto snapshots = messages(peer);
    ASSERT_EQ(snapshots.size(), 1U);
    const auto& payload = std::get<SnapshotPayload>(snapshots.front());
    EXPECT_EQ(payload.server_tick, 1U);
    ASSERT_EQ(payload.entities.size(), 1U);
    const auto pos_id = accept.strings.id_of(StringCategory::field, "Position.pos");
    ASSERT_TRUE(pos_id.has_value());
    EXPECT_EQ(payload.entities.front().fields,
              (std::vector<engine::network::FieldState>{
                  {.field_id = *pos_id, .value = engine::core::Vec2{3.F, 4.F}}}));
}

TEST_F(NetworkServerTest, IgnoresPacketsBeforeStarting) {
    auto& peer = open(loopback.hub.add_client());

    send(peer, server_peer, ConnectRequestPayload{.client_nonce = 7});
    ASSERT_TRUE(loopback.start());
    server.receive();

    EXPECT_EQ(server.player_count(), 0U);
    EXPECT_TRUE(messages(peer).empty());
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
    LoopbackServer refusing(clock, bus, listed, ServerOptions{.max_players = 1});
    std::error_code ec;

    EXPECT_FALSE(refusing.server.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, StartRejectsReplicatedFieldNamesTooLargeForOnePacket) {
    const auto many_fields = test_fields::registry();
    LoopbackServer crowded(
        clock, bus, many_fields,
        ServerOptions{.max_players = 1, .strings = payload_samples::long_name_strings(17)});
    LoopbackServer roomy(
        clock, bus, no_fields,
        ServerOptions{.max_players = 1, .strings = payload_samples::long_name_strings(17)});
    std::error_code ec;

    EXPECT_FALSE(crowded.server.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_TRUE(roomy.server.start(ec)) << ec.message();
}

TEST_F(NetworkServerTest, StartRejectsAStringTableTooLargeForOnePacket) {
    LoopbackServer crowded(
        clock, bus, no_fields,
        ServerOptions{.max_players = 1, .strings = payload_samples::long_name_strings(18)});
    std::error_code ec;

    EXPECT_FALSE(crowded.server.start(ec));
    EXPECT_EQ(ec, std::errc::invalid_argument);
}

TEST_F(NetworkServerTest, SendsTheStringTableAndTheCurrentMusicToPlayersJoiningLater) {
    const auto strings = payload_samples::strings_of(
        {{.category = StringCategory::sound, .id = 1, .name = "shot.wav"},
         {.category = StringCategory::music, .id = 2, .name = "stage.ogg"}});
    LoopbackServer audio(clock, bus, no_fields,
                         ServerOptions{.max_players = 1, .strings = strings});
    ASSERT_TRUE(audio.start());
    bus.publish(MusicChanged{.music_id = 2});

    auto& peer = audio.join();
    const auto accepted = messages(peer);

    ASSERT_EQ(accepted.size(), 1U);
    const auto& payload = std::get<ConnectAcceptPayload>(accepted.front());
    EXPECT_EQ(payload.strings, strings);
    EXPECT_EQ(payload.current_music, 2);
}

TEST_F(NetworkServerTest, SendsMusicChangesToPlayers) {
    ASSERT_TRUE(loopback.start());
    auto& peer = loopback.join();
    ASSERT_EQ(messages(peer).size(), 1U);
    EntityManager registry;

    bus.publish(MusicChanged{.music_id = engine::network::no_music});
    server.send_snapshot(registry);

    const auto events = messages(peer);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front(),
              Message{EventPayload{MusicEvent{.music_id = engine::network::no_music}}});
}

TEST_F(NetworkServerTest, SendsItsStringTableAndPublishedEventsToPlayers) {
    const auto strings = payload_samples::strings_of(
        {{.category = StringCategory::type, .id = 3, .name = "bydo"},
         {.category = StringCategory::event, .id = 2, .name = "boom"},
         {.category = StringCategory::event_key, .id = 5, .name = "at"}});
    LoopbackServer typed(clock, bus, no_fields,
                         ServerOptions{.max_players = 1, .strings = strings});
    ASSERT_TRUE(typed.start());
    auto& peer = typed.join();
    const auto accepted = messages(peer);
    ASSERT_EQ(accepted.size(), 1U);
    EXPECT_EQ(std::get<ConnectAcceptPayload>(accepted.front()).strings, strings);
    EntityManager registry;

    bus.publish(NamedEvent{.name = "missing", .arguments = {}});
    bus.publish(
        NamedEvent{.name = "boom", .arguments = {{.key = "at", .value = Vec2{1.0F, 2.0F}}}});
    typed.server.send_snapshot(registry);

    const auto received = messages(peer);
    ASSERT_EQ(received.size(), 2U);
    const NamedEventState boom{.name_id = 2,
                               .arguments = {{.key_id = 5, .value = Vec2{1.0F, 2.0F}}}};
    EXPECT_EQ(received[0], Message{EventPayload{boom}});
    EXPECT_TRUE(std::holds_alternative<SnapshotPayload>(received[1]));
}

TEST_F(NetworkServerTest, ReportsStatsForEachConnectedPlayer) {
    ASSERT_TRUE(loopback.start());
    EXPECT_TRUE(server.stats().empty());

    static_cast<void>(loopback.join());

    const auto stats = server.stats();
    ASSERT_EQ(stats.size(), 1U);
    EXPECT_EQ(stats.front().player_id, 1U);
    EXPECT_EQ(stats.front().network.packets_lost, 0U);
}

TEST_F(NetworkServerTest, StopTellsConnectedPlayersTheServerIsShuttingDown) {
    ASSERT_TRUE(loopback.start());
    auto& peer = loopback.join();
    ASSERT_EQ(messages(peer).size(), 1U);

    server.stop();

    const auto goodbye = messages(peer);
    ASSERT_EQ(goodbye.size(), 1U);
    EXPECT_EQ(goodbye.front(),
              Message{DisconnectPayload{.reason = DisconnectReason::server_shutdown}});
    EXPECT_EQ(server.player_count(), 0U);
}

TEST(NetworkServerPeers, ForgetsPeersThatHaveNoSession) {
    ManualClock clock;
    EventBus bus;
    const engine::core::FieldRegistry fields;
    SpyTransport transport;
    NetworkServer server(clock, bus, fields, transport, ServerOptions{.max_players = 1});
    std::error_code ec;
    ASSERT_TRUE(server.start(ec));

    transport.spy.inbox = {inbound(PeerId{1}, ConnectRequestPayload{.client_nonce = 1}),
                           inbound(PeerId{2}, ConnectRequestPayload{.client_nonce = 2}),
                           inbound(PeerId{3}, InputPayload{})};
    server.receive();

    EXPECT_EQ(server.player_count(), 1U);
    EXPECT_EQ(transport.spy.forgotten, (std::vector<PeerId>{PeerId{2}, PeerId{3}}));
    ASSERT_EQ(transport.spy.sent.size(), 2U);
    EXPECT_EQ(transport.spy.sent[1].to, PeerId{2});
}

TEST(NetworkServerPeers, ForgetsAPlayerOnceItsSessionEnds) {
    ManualClock clock;
    EventBus bus;
    const engine::core::FieldRegistry fields;
    SpyTransport transport;
    NetworkServer server(clock, bus, fields, transport, ServerOptions{.max_players = 1});
    std::error_code ec;
    ASSERT_TRUE(server.start(ec));
    transport.spy.inbox = {inbound(PeerId{1}, ConnectRequestPayload{.client_nonce = 1})};
    server.receive();
    ASSERT_TRUE(transport.spy.forgotten.empty());

    transport.spy.inbox = {
        inbound(PeerId{1}, DisconnectPayload{.reason = DisconnectReason::normal})};
    server.receive();

    EXPECT_EQ(server.player_count(), 0U);
    EXPECT_EQ(transport.spy.forgotten, std::vector<PeerId>{PeerId{1}});
}
