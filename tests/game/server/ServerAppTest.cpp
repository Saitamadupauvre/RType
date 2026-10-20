#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "ServerApp.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Server.hpp"

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

engine::core::Vec2 position_of(ServerApp& app, std::uint32_t player_id) {
    return app.registry().get<engine::core::Position>(*app.player_entity(player_id)).pos;
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

TEST(ServerApp, JoiningPlayerGetsAReplicatedSquare) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);

    app.events().publish(PlayerJoined{.player_id = 1});

    const auto entity = app.player_entity(1);
    ASSERT_TRUE(entity.has_value());
    EXPECT_TRUE(app.registry().has<engine::network::Replicated>(*entity));
    EXPECT_TRUE(app.registry().has<engine::core::Position>(*entity));
}

TEST(ServerApp, PlayersSpawnAtDifferentPositions) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);

    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});

    EXPECT_NE(position_of(app, 1).y, position_of(app, 2).y);
}

TEST(ServerApp, DuplicateJoinKeepsTheFirstSquare) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto first = app.player_entity(1);

    app.events().publish(PlayerJoined{.player_id = 1});

    EXPECT_EQ(app.player_entity(1), first);
}

TEST(ServerApp, LeavingPlayerSquareIsDestroyed) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto entity = *app.player_entity(1);

    app.events().publish(PlayerLeft{.player_id = 1});

    EXPECT_FALSE(app.player_entity(1).has_value());
    EXPECT_FALSE(app.registry().alive(entity));
}

TEST(ServerApp, UnknownPlayerLeavingIsIgnored) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});

    app.events().publish(PlayerLeft{.player_id = 7});

    EXPECT_TRUE(app.player_entity(1).has_value());
}

TEST(ServerApp, InputMovesOnlyThatPlayerSquare) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    app.events().publish(PlayerJoined{.player_id = 2});
    const auto start_one = position_of(app, 1);
    const auto start_two = position_of(app, 2);

    app.apply_inputs({PlayerInput{.player_id = 1, .action_flags = 0, .move_x = 127, .move_y = 0}});
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

    app.apply_inputs({PlayerInput{.player_id = 1, .action_flags = 0, .move_x = 0, .move_y = -128}});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).y, start.y - 1.F);
}

TEST(ServerApp, InputFromUnknownPlayerIsIgnored) {
    const ManualClock clock;
    ServerApp app(clock, config, players_scripts);
    app.events().publish(PlayerJoined{.player_id = 1});
    const auto start = position_of(app, 1);

    app.apply_inputs({PlayerInput{.player_id = 9, .action_flags = 0, .move_x = 127, .move_y = 0}});
    app.tick(tick_step);

    EXPECT_FLOAT_EQ(position_of(app, 1).x, start.x);
}

TEST(ServerApp, JoinWithoutScriptsSpawnsNothing) {
    const ManualClock clock;
    ServerApp app(clock, config, std::filesystem::path(RTYPE_FIXTURES_DIR) / "missing");

    app.events().publish(PlayerJoined{.player_id = 1});

    EXPECT_FALSE(app.player_entity(1).has_value());
}
