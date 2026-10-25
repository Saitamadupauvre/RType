#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr float bydo_speed = 300.F;
constexpr float bydo_width = 63.F;
constexpr float bydo_height = 72.F;
constexpr float shot_size = 24.F;
constexpr float first_shot_delay = 0.8F;
constexpr float fire_interval = 2.5F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

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

    [[nodiscard]] std::vector<Entity> shots() {
        std::vector<Entity> found;
        _world.query<Tags>().each([&](Entity entity, const Tags& tags) {
            if (tags.has("enemy_shot")) {
                found.push_back(entity);
            }
        });
        return found;
    }

    [[nodiscard]] std::size_t shots_heard() {
        const auto commands = _runtime.take_audio_commands();
        return static_cast<std::size_t>(
            std::ranges::count_if(commands, [](const engine::script::AudioCommand& command) {
                return command.name == "laser_shoot.wav";
            }));
    }
};

} // namespace

TEST_F(BydoTest, IsDrawnWithItsSprite) {
    const Entity bydo = bydo_at({1000.F, 300.F});
    ASSERT_TRUE(_world.has<Sprite>(bydo));
    EXPECT_EQ(_world.get<Sprite>(bydo).path, "sprites/bydo.png");
}

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

TEST_F(BydoTest, HoldsFireUntilTheFirstShotDelay) {
    bydo_at({1000.F, 300.F});
    tick(first_shot_delay - 0.1F);
    EXPECT_TRUE(shots().empty());
    EXPECT_EQ(shots_heard(), 0U);
}

TEST_F(BydoTest, FiresAShotStraightAheadFromItsMiddle) {
    const Entity bydo = bydo_at({1000.F, 300.F});
    tick(first_shot_delay);

    const auto fired = shots();
    ASSERT_EQ(fired.size(), 1U);
    const Vec2 bydo_position = _world.get<Position>(bydo).pos;
    EXPECT_FLOAT_EQ(_world.get<Position>(fired.front()).pos.x, bydo_position.x - shot_size);
    EXPECT_FLOAT_EQ(_world.get<Position>(fired.front()).pos.y,
                    bydo_position.y + ((bydo_height - shot_size) / 2.F));
    EXPECT_EQ(shots_heard(), 1U);
}

TEST_F(BydoTest, KeepsFiringAtItsInterval) {
    bydo_at({1000.F, 300.F});
    tick(first_shot_delay);
    EXPECT_EQ(shots_heard(), 1U);
    tick(fire_interval - 0.1F);
    EXPECT_EQ(shots_heard(), 0U);
    tick(0.1F);
    EXPECT_EQ(shots_heard(), 1U);
}
