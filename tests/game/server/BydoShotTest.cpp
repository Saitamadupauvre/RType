#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/ScriptRuntime.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr float shot_speed = 700.F;
constexpr float shot_size = 24.F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

class BydoShotTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
    }

    Entity shot_at(Vec2 at) {
        const auto shot = _runtime.spawn("bydo_shot", at);
        if (!shot) {
            throw std::runtime_error("cannot spawn a bydo shot");
        }
        return *shot;
    }

    void tick(float dt) {
        _runtime.update(dt);
        _runtime.flush_destroyed();
    }
};

} // namespace

TEST_F(BydoShotTest, IsAnEnemyShotDrawnWithItsSprite) {
    const Entity shot = shot_at({1000.F, 300.F});
    EXPECT_EQ(_world.get<Sprite>(shot).path, "sprites/bydo_shot.png");
    EXPECT_TRUE(_world.get<Tags>(shot).has("enemy_shot"));
    EXPECT_FALSE(_world.get<Tags>(shot).has("enemy"));
}

TEST_F(BydoShotTest, MovesLeftAtItsSpeed) {
    const Entity shot = shot_at({1000.F, 300.F});
    tick(0.5F);
    EXPECT_FLOAT_EQ(_world.get<Position>(shot).pos.x, 1000.F - (shot_speed * 0.5F));
    EXPECT_FLOAT_EQ(_world.get<Position>(shot).pos.y, 300.F);
}

TEST_F(BydoShotTest, StaysAliveWhilePartlyVisible) {
    const Entity shot = shot_at({0.F, 300.F});
    tick(0.01F);
    EXPECT_TRUE(_world.alive(shot));
}

TEST_F(BydoShotTest, IsDestroyedOnceItLeavesTheScreenOnTheLeft) {
    const Entity shot = shot_at({-shot_size + 1.F, 300.F});
    tick(0.01F);
    EXPECT_FALSE(_world.alive(shot));
}
