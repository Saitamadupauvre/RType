#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "ServerApp.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::time::Duration;
using engine::core::time::ManualClock;
using engine::network::PlayerInput;
using engine::network::PlayerJoined;
using engine::network::PlayerLeft;
using rtype::server::ServerApp;
using rtype::server::ServerConfig;

namespace {

constexpr Duration tick_step = std::chrono::nanoseconds(1'000'000'000 / 60);
constexpr ServerConfig config{.port = 4242, .max_players = 4};
const std::filesystem::path players_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "players";

std::vector<Entity> owned_by(ServerApp& app, std::uint32_t player_id) {
    std::vector<Entity> owned;
    app.registry().query<engine::core::Position>().each(
        [&](Entity entity, const engine::core::Position&) {
            if (app.script().entity_owner(entity) == player_id) {
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
    return app.registry().get<engine::core::Position>(ship_of(app, player_id)).pos;
}

PlayerInput input_for(std::uint32_t player_id, std::int8_t move_x, std::int8_t move_y) {
    return {.player_id = player_id, .action_flags = 0, .move_x = move_x, .move_y = move_y};
}

} // namespace

TEST(ServerApp, RegistersTheComponentsSentInSnapshots) {
    const ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});

    EXPECT_TRUE(app.registry().is_registered<engine::network::Replicated>());
    EXPECT_TRUE(app.registry().is_registered<engine::core::Position>());
    EXPECT_TRUE(app.registry().is_registered<engine::core::Velocity>());
}

TEST(ServerApp, TickRunsSystemsWithTheFixedStep) {
    const ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});
    std::vector<float> steps;
    app.systems().add("record", 0, [&steps](EntityManager&, float dt) { steps.push_back(dt); });

    app.tick(tick_step);

    ASSERT_EQ(steps.size(), 1U);
    EXPECT_FLOAT_EQ(steps.front(), 1.0F / 60.0F);
}

TEST(ServerApp, RunTicksAtFixedRateUntilAskedToStop) {
    ManualClock clock;
    ServerApp app(clock, ServerConfig{.port = 4242, .max_players = 4});
    int ticks = 0;
    app.systems().add("count", 0, [&ticks](EntityManager&, float) { ++ticks; });
    int checks = 0;

    app.run([&] {
        clock.advance(tick_step * 2);
        return ++checks > 3;
    });

    EXPECT_EQ(ticks, 6);
}

TEST(ServerApp, StartReportsPortAlreadyInUse) {
    const ManualClock clock;
    ServerApp first(clock, ServerConfig{.port = 0, .max_players = 4});
    std::string error;
    ASSERT_TRUE(first.start(error)) << error;
    ServerApp second(clock, ServerConfig{.port = first.network().port(), .max_players = 4});

    EXPECT_FALSE(second.start(error));
    EXPECT_NE(error.find(std::to_string(first.network().port())), std::string::npos) << error;
}

TEST(ServerApp, TypeTableListsDeclaredEntitiesInOrder) {
    const ManualClock clock;
    const ServerApp app(clock, config, players_scripts);

    ASSERT_EQ(app.types().size(), 2U);
    EXPECT_EQ(app.types()[0].name, "scenery");
    EXPECT_EQ(app.types()[0].type_id, 1U);
    EXPECT_EQ(app.types()[1].name, "square");
    EXPECT_EQ(app.types()[1].type_id, 2U);
}

TEST(ServerApp, JoiningPlayerGetsTheEntitiesOfOnPlayerJoined) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);

    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});

    EXPECT_EQ(owned_by(app, 1).size(), 1U);
    EXPECT_NE(position_of(app, 1).y, position_of(app, 2).y);
}

TEST(ServerApp, ReplicateTagsScriptEntitiesWithTypeAndOwner) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 3});

    app.replicate();

    const auto& replicated = app.registry().get<engine::network::Replicated>(ship_of(app, 3));
    EXPECT_EQ(replicated.type_id, 2U);
    EXPECT_EQ(replicated.owner, 3U);
}

TEST(ServerApp, ReplicateClearsOwnerOnceThePlayerHasLeft) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const Entity ship = ship_of(app, 1);
    app.replicate();
    app.script().run_string("for _, e in ipairs(find_player(1):entities()) do e.owner = nil end",
                            "test");

    app.replicate();

    EXPECT_EQ(app.registry().get<engine::network::Replicated>(ship).owner, 0U);
}

TEST(ServerApp, EntitiesNotSpawnedByScriptsAreNotReplicated) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    const Entity native = app.registry().spawn();
    app.registry().add(native, engine::core::Position{});

    app.replicate();

    EXPECT_FALSE(app.registry().has<engine::network::Replicated>(native));
}

TEST(ServerApp, LeavingPlayerEntitiesAreDestroyedOnTheNextTick) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const Entity ship = ship_of(app, 1);

    app.events().publish(PlayerLeft{.player_id = 1});
    app.tick(tick_step);

    EXPECT_FALSE(app.registry().alive(ship));
}

TEST(ServerApp, InputMovesOnlyThatPlayerEntity) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});
    const auto start_one = position_of(app, 1);
    const auto start_two = position_of(app, 2);

    app.apply_inputs({input_for(1, 127, 0)});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).x, start_one.x + 1.F);
    EXPECT_FLOAT_EQ(position_of(app, 1).y, start_one.y);
    EXPECT_FLOAT_EQ(position_of(app, 2).x, start_two.x);
}

TEST(ServerApp, InputAxisIsClampedToUnitRange) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto start = position_of(app, 1);

    app.apply_inputs({input_for(1, 0, -128)});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).y, start.y - 1.F);
}

TEST(ServerApp, InputFromUnknownPlayerIsIgnored) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto start = position_of(app, 1);

    app.apply_inputs({input_for(9, 127, 0)});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).x, start.x);
}

TEST(ServerApp, MissingScriptsGiveAnEmptyGame) {
    const ManualClock clock;
    ServerApp app(clock, config, std::filesystem::path(RTYPE_FIXTURES_DIR) / "missing");

    app.events().publish(PlayerJoined{.player_id = 1});
    app.tick(tick_step);

    EXPECT_TRUE(app.types().empty());
    EXPECT_TRUE(owned_by(app, 1).empty());
}
