#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/app/ServerApp.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/network/Transport.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

constexpr float ship_start_x = 160.F;
constexpr float spawn_interval = 1.5F;
constexpr float restart_delay = 3.F;
constexpr float frame = 1.F / 60.F;
constexpr float world_width = 1920.F;
constexpr std::uint32_t first_player = 1;
constexpr std::uint32_t second_player = 2;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

class GameFlowTest : public ::testing::Test {
protected:
    engine::core::time::ManualClock _clock;
    engine::network::LoopbackNetwork _loopback;
    engine::app::ServerApp _app{_clock, _loopback.server(), {.scripts = game_scripts}};
    EntityManager& _world = _app.world();
    ScriptRuntime& _runtime = _app.script();
    ServerApi& _server = _app.script_server();

    void SetUp() override { ASSERT_TRUE(_runtime.spawn("game").has_value()); }

    void tick(float dt = frame) {
        _app.tick(std::chrono::duration_cast<engine::core::time::Duration>(
            std::chrono::duration<float>(dt)));
    }

    void join(std::uint32_t player_id) { ASSERT_TRUE(_server.player_joined(player_id)); }

    void leave(std::uint32_t player_id) {
        ASSERT_TRUE(_server.player_left(player_id));
        tick();
    }

    [[nodiscard]] std::vector<Entity> tagged(const std::string& label) {
        std::vector<Entity> found;
        _world.query<Tags>().each([&](Entity entity, const Tags& tags) {
            if (tags.has(label)) {
                found.push_back(entity);
            }
        });
        return found;
    }

    [[nodiscard]] std::size_t just_spawned_bydos() {
        std::size_t count = 0;
        _world.query<Tags, Position>().each(
            [&](Entity, const Tags& tags, const Position& position) {
                count += tags.has("enemy") && position.pos.x == world_width ? 1 : 0;
            });
        return count;
    }

    [[nodiscard]] Entity ship_of(std::uint32_t player_id) {
        for (const Entity ship : tagged("ship")) {
            if (_server.entity_owner(ship) == player_id) {
                return ship;
            }
        }
        throw std::runtime_error("player " + std::to_string(player_id) + " has no ship");
    }

    [[nodiscard]] bool has_ship(std::uint32_t player_id) {
        for (const Entity ship : tagged("ship")) {
            if (_server.entity_owner(ship) == player_id) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] Vec2 position_of(std::uint32_t player_id) {
        return _world.get<Position>(ship_of(player_id)).pos;
    }

    void steer(std::uint32_t player_id, float move_x) {
        InputState input;
        if (move_x > 0.F) {
            input.keys.insert(engine::core::Key::Right);
        } else if (move_x < 0.F) {
            input.keys.insert(engine::core::Key::Left);
        }
        _server.set_player_input(player_id, input);
    }

    Entity spawn(const std::string& type, Vec2 at) {
        const auto entity = _runtime.spawn(type, at);
        if (!entity) {
            throw std::runtime_error("cannot spawn " + type);
        }
        return *entity;
    }

    void kill_ship_of(std::uint32_t player_id) {
        const Vec2 at = position_of(player_id);
        spawn("bydo", {at.x + 10.F, at.y + 10.F});
        tick();
        tick();
    }

    void lose() {
        for (const Entity ship : tagged("ship")) {
            const Vec2 at = _world.get<Position>(ship).pos;
            spawn("bydo", {at.x + 10.F, at.y + 10.F});
        }
        tick();
        tick();
    }
};

} // namespace

TEST_F(GameFlowTest, WaitsWithoutShipsNorBydosUntilAPlayerJoins) {
    tick(spawn_interval);
    tick(spawn_interval);

    EXPECT_TRUE(tagged("ship").empty());
    EXPECT_TRUE(tagged("enemy").empty());
}

TEST_F(GameFlowTest, FirstPlayerToJoinStartsTheGameWithTheirShip) {
    join(first_player);

    EXPECT_FLOAT_EQ(position_of(first_player).x, ship_start_x);
    tick(spawn_interval);
    EXPECT_EQ(tagged("enemy").size(), 1U);
}

TEST_F(GameFlowTest, PlayerJoiningDuringTheGameGetsAShipOnAnotherRow) {
    join(first_player);
    tick();

    join(second_player);

    EXPECT_EQ(tagged("ship").size(), 2U);
    EXPECT_FLOAT_EQ(position_of(second_player).x, ship_start_x);
    EXPECT_NE(position_of(first_player).y, position_of(second_player).y);
}

TEST_F(GameFlowTest, EachShipIsTintedWithTheColorOfItsSlot) {
    join(first_player);
    join(second_player);

    const Rgba first = _world.get<Sprite>(ship_of(first_player)).color;
    const Rgba second = _world.get<Sprite>(ship_of(second_player)).color;

    EXPECT_NE(first, second);
    EXPECT_NE(first, Rgba{});
}

TEST_F(GameFlowTest, EachPlayerSteersOnlyTheirOwnShip) {
    join(first_player);
    join(second_player);
    const Vec2 second_start = position_of(second_player);

    steer(first_player, 1.F);
    tick();

    EXPECT_GT(position_of(first_player).x, ship_start_x);
    EXPECT_FLOAT_EQ(position_of(second_player).x, second_start.x);
}

TEST_F(GameFlowTest, GameGoesOnWhileOneShipIsLeft) {
    join(first_player);
    join(second_player);

    kill_ship_of(first_player);

    EXPECT_FALSE(has_ship(first_player));
    EXPECT_TRUE(has_ship(second_player));
    tick(spawn_interval);
    EXPECT_EQ(just_spawned_bydos(), 1U);
}

TEST_F(GameFlowTest, LosingEveryShipStopsTheBydos) {
    join(first_player);
    join(second_player);

    lose();

    ASSERT_TRUE(tagged("ship").empty());
    tick(spawn_interval);
    EXPECT_EQ(just_spawned_bydos(), 0U);
}

TEST_F(GameFlowTest, RestartsAfterTheDelayWithAShipForEveryConnectedPlayer) {
    join(first_player);
    join(second_player);
    lose();

    tick(restart_delay - frame);
    EXPECT_TRUE(tagged("ship").empty());
    tick(frame);

    EXPECT_TRUE(has_ship(first_player));
    EXPECT_TRUE(has_ship(second_player));
}

TEST_F(GameFlowTest, RestartClearsLeftoverBydosAndMissiles) {
    join(first_player);
    spawn("missile", {1000.F, 100.F});
    lose();

    tick(restart_delay);

    EXPECT_TRUE(tagged("enemy").empty());
    EXPECT_TRUE(tagged("missile").empty());
}

TEST_F(GameFlowTest, RestartedGameSpawnsBydosAgain) {
    join(first_player);
    lose();
    tick(restart_delay);

    tick(spawn_interval);

    EXPECT_EQ(tagged("enemy").size(), 1U);
}

TEST_F(GameFlowTest, LeavingPlayerShipIsRemoved) {
    join(first_player);
    join(second_player);

    leave(first_player);

    EXPECT_FALSE(has_ship(first_player));
    EXPECT_TRUE(has_ship(second_player));
}

TEST_F(GameFlowTest, GameWaitsAgainOnceEveryPlayerHasLeft) {
    join(first_player);
    leave(first_player);
    tick();
    tick(restart_delay);

    tick(spawn_interval);

    EXPECT_TRUE(tagged("ship").empty());
    EXPECT_TRUE(tagged("enemy").empty());
}

TEST_F(GameFlowTest, PlayerJoiningAfterEveryoneLeftStartsANewGame) {
    join(first_player);
    leave(first_player);
    tick();
    tick(restart_delay);

    join(second_player);

    EXPECT_TRUE(has_ship(second_player));
}

TEST_F(GameFlowTest, PlayerJoiningDuringTheGameOverPlaysFromTheRestart) {
    join(first_player);
    lose();
    join(second_player);
    EXPECT_FALSE(has_ship(second_player));

    tick(restart_delay);

    EXPECT_TRUE(has_ship(second_player));
}

TEST_F(GameFlowTest, CanLoseAndRestartSeveralTimes) {
    join(first_player);
    for (int round = 0; round < 3; ++round) {
        lose();
        tick(restart_delay);
        ASSERT_EQ(tagged("ship").size(), 1U);
    }
}
