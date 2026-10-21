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
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr float bydo_speed = 300.F;
constexpr float bydo_width = 64.F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "client" / "single_player";

class BydoTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
    }

    Entity bydo_at(Vec2 at) {
        const auto bydo = _runtime.spawn("bydo", at);
        if (!bydo) {
            throw std::runtime_error("cannot spawn a bydo");
        }
        return *bydo;
    }

    void tick(float dt) {
        _runtime.update(dt);
        _runtime.flush_destroyed();
    }
};

} // namespace

TEST_F(BydoTest, IsDrawn) { EXPECT_TRUE(_world.has<Sprite>(bydo_at({1000.F, 300.F}))); }

TEST_F(BydoTest, MovesLeftAtItsSpeed) {
    const Entity bydo = bydo_at({1000.F, 300.F});
    tick(0.5F);
    EXPECT_FLOAT_EQ(_world.get<Position>(bydo).pos.x, 1000.F - (bydo_speed * 0.5F));
    EXPECT_FLOAT_EQ(_world.get<Position>(bydo).pos.y, 300.F);
}

TEST_F(BydoTest, StaysAliveWhilePartlyVisible) {
    const Entity bydo = bydo_at({0.F, 300.F});
    tick(0.1F);
    EXPECT_GT(_world.get<Position>(bydo).pos.x, -bydo_width);
    EXPECT_TRUE(_world.alive(bydo));
}

TEST_F(BydoTest, IsDestroyedOnceItLeavesTheScreenOnTheLeft) {
    const Entity bydo = bydo_at({10.F, 300.F});
    tick(0.5F);
    EXPECT_FALSE(_world.alive(bydo));
}
