#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/InputState.hpp"
#include "engine/script/ScriptRuntime.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::InputState;
using engine::script::ScriptRuntime;

namespace {

constexpr Vec2 ship_start{160.F, 522.F};
constexpr float spawn_interval = 1.5F;
constexpr float frame = 1.F / 60.F;
constexpr float world_width = 1920.F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "client" / "single_player";

class GameFlowTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
        ASSERT_TRUE(_runtime.spawn("game").has_value());
    }

    void tick(float dt = frame) {
        _runtime.update(dt);
        _runtime.flush_destroyed();
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

    [[nodiscard]] Entity ship() {
        const auto ships = tagged("ship");
        if (ships.size() != 1) {
            throw std::runtime_error("expected exactly one ship");
        }
        return ships.front();
    }

    void press_fire(bool pressed) {
        InputState input;
        if (pressed) {
            input.pressed.emplace("fire");
        }
        for (const Entity player : tagged("player")) {
            _runtime.set_input(player, input);
        }
    }

    Entity spawn(const std::string& type, Vec2 at) {
        const auto entity = _runtime.spawn(type, at);
        if (!entity) {
            throw std::runtime_error("cannot spawn " + type);
        }
        return *entity;
    }

    void lose() {
        const Vec2 at = _world.get<Position>(ship()).pos;
        spawn("bydo", {at.x + 10.F, at.y + 10.F});
        tick();
        tick();
    }
};

} // namespace

TEST_F(GameFlowTest, StartsWithOneShipAtItsStartPosition) {
    const Vec2 at = _world.get<Position>(ship()).pos;
    EXPECT_FLOAT_EQ(at.x, ship_start.x);
    EXPECT_FLOAT_EQ(at.y, ship_start.y);
}

TEST_F(GameFlowTest, SpawnsBydosWhilePlaying) {
    tick(spawn_interval);
    EXPECT_EQ(tagged("enemy").size(), 1U);
}

TEST_F(GameFlowTest, LosingTheShipStopsTheBydos) {
    lose();
    ASSERT_TRUE(tagged("ship").empty());
    for (int wave = 0; wave < 4; ++wave) {
        tick(spawn_interval);
        EXPECT_EQ(just_spawned_bydos(), 0U);
    }
}

TEST_F(GameFlowTest, HoldingFireThroughTheGameOverDoesNotRestart) {
    press_fire(true);
    lose();
    press_fire(true);
    tick();
    tick();
    EXPECT_TRUE(tagged("ship").empty());
}

TEST_F(GameFlowTest, ReleasingThenPressingFireRestartsTheGame) {
    lose();
    press_fire(false);
    tick();
    press_fire(true);
    tick();
    const Vec2 at = _world.get<Position>(ship()).pos;
    EXPECT_FLOAT_EQ(at.x, ship_start.x);
    EXPECT_FLOAT_EQ(at.y, ship_start.y);
}

TEST_F(GameFlowTest, RestartClearsLeftoverBydosAndMissiles) {
    spawn("missile", {1000.F, 100.F});
    lose();
    press_fire(false);
    tick();
    press_fire(true);
    tick();
    EXPECT_TRUE(tagged("enemy").empty());
    EXPECT_TRUE(tagged("missile").empty());
}

TEST_F(GameFlowTest, RestartedGameSpawnsBydosAgain) {
    lose();
    press_fire(false);
    tick();
    press_fire(true);
    tick();
    press_fire(false);
    tick(spawn_interval);
    EXPECT_EQ(tagged("enemy").size(), 1U);
}

TEST_F(GameFlowTest, CanLoseAndRestartSeveralTimes) {
    for (int round = 0; round < 3; ++round) {
        lose();
        press_fire(false);
        tick();
        press_fire(true);
        tick();
        ASSERT_EQ(tagged("ship").size(), 1U);
    }
}
