#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "engine/app/ServerApp.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/network/Transport.hpp"

using engine::app::ServerApp;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::time::Duration;
using engine::core::time::ManualClock;
using engine::network::LoopbackNetwork;
using engine::network::PlayerJoined;
using engine::network::PlayerLeft;
using engine::network::StringCategory;
using engine::network::StringEntry;
using engine::network::UdpServerTransport;

namespace {

constexpr Duration tick_step = std::chrono::nanoseconds(1'000'000'000 / 60);
const std::filesystem::path players_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "server" / "players";

std::vector<Entity> owned_by(ServerApp& app, std::uint32_t player_id) {
    std::vector<Entity> owned;
    app.world().query<engine::core::Position>().each(
        [&](Entity entity, const engine::core::Position&) {
            if (app.script_server().entity_owner(entity) == player_id) {
                owned.push_back(entity);
            }
        });
    return owned;
}

Entity ship_of(ServerApp& app, std::uint32_t player_id) {
    const auto owned = owned_by(app, player_id);
    if (owned.size() != 1) {
        throw std::runtime_error("expected one entity for player " + std::to_string(player_id));
    }
    return owned.front();
}

engine::core::Vec2 position_of(ServerApp& app, std::uint32_t player_id) {
    return app.world().get<engine::core::Position>(ship_of(app, player_id)).pos;
}

void press(ServerApp& app, std::uint32_t player_id, std::initializer_list<engine::core::Key> keys) {
    app.script_server().set_player_input(
        player_id, engine::core::InputState{.keys = keys, .mouse_buttons = {}, .pointer = {}});
}

struct Server {
    explicit Server(const std::filesystem::path& scripts = {})
        : app(clock, loopback.server(), {.scripts = scripts, .max_players = 4}) {}

    ManualClock clock;
    LoopbackNetwork loopback;
    ServerApp app;
};

} // namespace

TEST(ServerApp, RegistersTheComponentsSentInSnapshots) {
    Server server;
    ServerApp& app = server.app;

    EXPECT_TRUE(app.world().is_registered<engine::network::Replicated>());
    EXPECT_TRUE(app.world().is_registered<engine::core::Position>());
    EXPECT_TRUE(app.world().is_registered<engine::core::Sprite>());
}

TEST(ServerApp, TickRunsSystemsWithTheFixedStep) {
    Server server;
    ServerApp& app = server.app;
    std::vector<float> steps;
    app.systems().add("record", 0, [&steps](EntityManager&, float dt) { steps.push_back(dt); });

    app.tick(tick_step);

    ASSERT_EQ(steps.size(), 1U);
    EXPECT_FLOAT_EQ(steps.front(), 1.0F / 60.0F);
}

TEST(ServerApp, RunTicksAtFixedRateUntilAskedToStop) {
    Server server;
    ServerApp& app = server.app;
    int ticks = 0;
    app.systems().add("count", 0, [&ticks](EntityManager&, float) { ++ticks; });
    int checks = 0;

    app.run([&] {
        server.clock.advance(tick_step * 2);
        return ++checks > 3;
    });

    EXPECT_EQ(ticks, 6);
}

TEST(ServerApp, StartFailsOnAPortAlreadyInUse) {
    const ManualClock clock;
    UdpServerTransport first_transport(0);
    ServerApp first(clock, first_transport, {});
    std::error_code ec;
    ASSERT_TRUE(first.start(ec)) << ec.message();
    UdpServerTransport second_transport(first_transport.port());
    ServerApp second(clock, second_transport, {});

    EXPECT_FALSE(second.start(ec));
    EXPECT_TRUE(ec);
}

TEST(ServerApp, StartsOverALoopbackNetwork) {
    Server server;
    std::error_code ec;

    EXPECT_TRUE(server.app.start(ec)) << ec.message();
    server.app.stop();
}

TEST(ServerApp, StringTableListsDeclaredEntitiesInOrder) {
    const Server server(players_scripts);
    const ServerApp& app = server.app;

    EXPECT_EQ(
        app.strings().entries(StringCategory::type),
        (std::vector<StringEntry>{{.category = StringCategory::type, .id = 1, .name = "scenery"},
                                  {.category = StringCategory::type, .id = 2, .name = "square"}}));
}

TEST(ServerApp, JoiningPlayerGetsTheEntitiesOfOnPlayerJoined) {
    Server server(players_scripts);
    ServerApp& app = server.app;

    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});

    EXPECT_EQ(owned_by(app, 1).size(), 1U);
    EXPECT_NE(position_of(app, 1).y, position_of(app, 2).y);
}

TEST(ServerApp, TickTagsScriptEntitiesWithTypeAndOwner) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    app.events().publish(PlayerJoined{.player_id = 3});

    app.tick(tick_step);

    const auto& replicated = app.world().get<engine::network::Replicated>(ship_of(app, 3));
    EXPECT_EQ(replicated.type_id, 2U);
    EXPECT_EQ(replicated.owner, 3U);
}

TEST(ServerApp, TickClearsTheOwnerOfDisownedEntities) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    app.events().publish(PlayerJoined{.player_id = 1});
    const Entity ship = ship_of(app, 1);
    app.tick(tick_step);
    app.script().run_string("for _, e in ipairs(find_player(1):entities()) do e.owner = nil end",
                            "test");

    app.tick(tick_step);

    EXPECT_EQ(app.world().get<engine::network::Replicated>(ship).owner, 0U);
}

TEST(ServerApp, EntitiesNotSpawnedByScriptsAreNotReplicated) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    const Entity native = app.world().spawn();
    app.world().add(native, engine::core::Position{});

    app.tick(tick_step);

    EXPECT_FALSE(app.world().has<engine::network::Replicated>(native));
}

TEST(ServerApp, ScriptEntitiesWithoutSpriteOrColorAreNotReplicated) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    const auto scenery = app.script().spawn("scenery");
    ASSERT_TRUE(scenery.has_value());

    app.tick(tick_step);

    EXPECT_FALSE(app.world().has<engine::network::Replicated>(*scenery));
}

TEST(ServerApp, LeavingPlayerEntitiesAreDestroyedOnTheNextTick) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    app.events().publish(PlayerJoined{.player_id = 1});
    const Entity ship = ship_of(app, 1);

    app.events().publish(PlayerLeft{.player_id = 1});
    app.tick(tick_step);

    EXPECT_FALSE(app.world().alive(ship));
}

TEST(ServerApp, InputMovesOnlyThatPlayerEntity) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});
    const auto start_one = position_of(app, 1);
    const auto start_two = position_of(app, 2);

    press(app, 1, {engine::core::Key::Right});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).x, start_one.x + 1.F);
    EXPECT_FLOAT_EQ(position_of(app, 1).y, start_one.y);
    EXPECT_FLOAT_EQ(position_of(app, 2).x, start_two.x);
}

TEST(ServerApp, InputFromUnknownPlayerIsIgnored) {
    Server server(players_scripts);
    ServerApp& app = server.app;
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto start = position_of(app, 1);

    press(app, 9, {engine::core::Key::Right});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).x, start.x);
}

TEST(ServerApp, MissingScriptsGiveAnEmptyGame) {
    Server server(std::filesystem::path(RTYPE_FIXTURES_DIR) / "missing");
    ServerApp& app = server.app;

    app.events().publish(PlayerJoined{.player_id = 1});
    app.tick(tick_step);

    EXPECT_TRUE(app.strings().entries().empty());
    EXPECT_TRUE(owned_by(app, 1).empty());
}

namespace {

const std::filesystem::path audio_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "server" / "audio";

struct HeardAudio {
    std::vector<engine::network::SoundPlayed> sounds;
    std::vector<std::uint16_t> musics;
};

HeardAudio listen(ServerApp& app) {
    HeardAudio heard;
    const auto sound = app.events().subscribe<engine::network::SoundPlayed>(
        [&heard](const engine::network::SoundPlayed& event) { heard.sounds.push_back(event); });
    const auto music = app.events().subscribe<engine::network::MusicChanged>(
        [&heard](const engine::network::MusicChanged& event) {
            heard.musics.push_back(event.music_id);
        });
    app.tick(tick_step);
    app.events().unsubscribe(sound);
    app.events().unsubscribe(music);
    return heard;
}

} // namespace

TEST(ServerApp, StringTableListsDeclaredAudioNamesInOrder) {
    const Server server(audio_scripts);
    const ServerApp& app = server.app;

    EXPECT_EQ(app.strings().entries(StringCategory::sound),
              (std::vector<StringEntry>{
                  {.category = StringCategory::sound, .id = 1, .name = "boom.wav"},
                  {.category = StringCategory::sound, .id = 2, .name = "shot.wav"}}));
    EXPECT_EQ(app.strings().entries(StringCategory::music),
              (std::vector<StringEntry>{
                  {.category = StringCategory::music, .id = 1, .name = "stage.ogg"}}));
}

TEST(ServerApp, ScriptsWithoutAudioHaveNoAudioNames) {
    const Server server(players_scripts);
    const ServerApp& app = server.app;

    EXPECT_TRUE(app.strings().entries(StringCategory::sound).empty());
    EXPECT_TRUE(app.strings().entries(StringCategory::music).empty());
}

TEST(ServerApp, TickPublishesTheMusicStartedByOnStart) {
    Server server(audio_scripts);
    ServerApp& app = server.app;

    const auto heard = listen(app);

    EXPECT_TRUE(heard.sounds.empty());
    EXPECT_EQ(heard.musics, (std::vector<std::uint16_t>{1}));
    EXPECT_TRUE(listen(app).musics.empty());
}

TEST(ServerApp, TickPublishesQueuedSoundsAndMusicStops) {
    Server server(audio_scripts);
    ServerApp& app = server.app;
    app.tick(tick_step);
    ASSERT_TRUE(app.script().run_string(R"(play("boom.wav") music(nil))", "test"));

    const auto heard = listen(app);

    ASSERT_EQ(heard.sounds.size(), 1U);
    EXPECT_EQ(heard.sounds[0].sound_id, 1U);
    EXPECT_EQ(heard.sounds[0].source_entity_id, engine::network::no_source_entity);
    EXPECT_EQ(heard.musics, (std::vector<std::uint16_t>{engine::network::no_music}));
}

TEST(ServerApp, SoundPlayedByAnEntityCarriesItsIndex) {
    Server server(audio_scripts);
    ServerApp& app = server.app;
    const auto speaker = app.script().spawn("speaker");
    ASSERT_TRUE(speaker.has_value());
    ASSERT_TRUE(app.script().call(*speaker, "speak"));

    const auto heard = listen(app);

    ASSERT_EQ(heard.sounds.size(), 1U);
    EXPECT_EQ(heard.sounds[0].sound_id, 2U);
    EXPECT_EQ(heard.sounds[0].source_entity_id, speaker->index());
}

namespace {

const std::filesystem::path events_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "server" / "events";

std::vector<engine::network::NamedEvent> listen_events(ServerApp& app) {
    std::vector<engine::network::NamedEvent> heard;
    const auto subscription = app.events().subscribe<engine::network::NamedEvent>(
        [&heard](const engine::network::NamedEvent& event) { heard.push_back(event); });
    app.tick(tick_step);
    app.events().unsubscribe(subscription);
    return heard;
}

} // namespace

TEST(ServerApp, StringTableListsDeclaredEventNamesAndKeysInOrder) {
    const Server server(events_scripts);
    const ServerApp& app = server.app;

    EXPECT_EQ(
        app.strings().entries(StringCategory::event),
        (std::vector<StringEntry>{{.category = StringCategory::event, .id = 1, .name = "explosion"},
                                  {.category = StringCategory::event, .id = 2, .name = "ping"}}));
    EXPECT_EQ(app.strings().entries(StringCategory::event_key),
              (std::vector<StringEntry>{
                  {.category = StringCategory::event_key, .id = 1, .name = "at"},
                  {.category = StringCategory::event_key, .id = 2, .name = "size"}}));
}

TEST(ServerApp, ScriptsWithoutEventsHaveNoEventNames) {
    const Server server(players_scripts);
    const ServerApp& app = server.app;

    EXPECT_TRUE(app.strings().entries(StringCategory::event).empty());
    EXPECT_TRUE(app.strings().entries(StringCategory::event_key).empty());
}

TEST(ServerApp, TickPublishesEmittedEventsInOrder) {
    Server server(events_scripts);
    ServerApp& app = server.app;
    ASSERT_TRUE(app.script().run_string(
        R"(emit("explosion", { at = { x = 4, y = 5 }, size = 2 }) emit("ping"))", "test"));

    const auto heard = listen_events(app);

    EXPECT_EQ(heard, (std::vector<engine::network::NamedEvent>{
                         {.name = "explosion",
                          .arguments = {{.key = "at", .value = engine::core::Vec2{4.0F, 5.0F}},
                                        {.key = "size", .value = 2.0F}}},
                         {.name = "ping", .arguments = {}}}));
    EXPECT_TRUE(listen_events(app).empty());
}

TEST(ServerApp, TickDestroysEntitiesWhoseHitboxesOverlap) {
    Server server(std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "server" / "collide");
    ServerApp& app = server.app;
    const auto bullet = app.script().spawn("bullet", engine::core::Vec2{1000.F, 300.F});
    const auto target = app.script().spawn("target", engine::core::Vec2{1000.F, 290.F});
    const auto far_target = app.script().spawn("target", engine::core::Vec2{1000.F, 800.F});
    ASSERT_TRUE(bullet && target && far_target);

    app.tick(tick_step);

    EXPECT_FALSE(app.world().alive(*bullet));
    EXPECT_FALSE(app.world().alive(*target));
    EXPECT_TRUE(app.world().alive(*far_target));
}

TEST(ServerApp, TickMovesScriptEntitiesByTheirVelocity) {
    Server server(std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "server" / "drift");
    ServerApp& app = server.app;
    const auto drifter = app.script().spawn("drifter", engine::core::Vec2{100.F, 100.F});
    ASSERT_TRUE(drifter);

    app.tick(tick_step);

    const auto position = app.world().get<engine::core::Position>(*drifter).pos;
    EXPECT_FLOAT_EQ(position.x, 101.F);
    EXPECT_FLOAT_EQ(position.y, 99.5F);
}
