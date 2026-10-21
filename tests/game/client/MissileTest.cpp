#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/ScriptRuntime.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr float missile_speed = 1200.F;
constexpr float world_width = 1920.F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "client" / "single_player";

class MissileTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
    }

    Entity launch(Vec2 at) {
        const auto missile = _runtime.spawn("missile", at);
        if (!missile) {
            throw std::runtime_error("cannot spawn a missile");
        }
        return *missile;
    }

    void tick(float dt) {
        _runtime.update(dt);
        _runtime.flush_destroyed();
    }
};

} // namespace

TEST_F(MissileTest, MovesRightAtItsSpeed) {
    const Entity missile = launch({100.F, 300.F});
    tick(0.1F);
    EXPECT_FLOAT_EQ(_world.get<Position>(missile).pos.x, 100.F + (missile_speed * 0.1F));
    EXPECT_FLOAT_EQ(_world.get<Position>(missile).pos.y, 300.F);
}

TEST_F(MissileTest, StaysAliveWhileOnScreen) {
    const Entity missile = launch({world_width - 200.F, 300.F});
    tick(0.1F);
    EXPECT_TRUE(_world.alive(missile));
}

TEST_F(MissileTest, IsDestroyedOnceItLeavesTheScreen) {
    const Entity missile = launch({world_width - 10.F, 300.F});
    tick(0.1F);
    EXPECT_FALSE(_world.alive(missile));
}
