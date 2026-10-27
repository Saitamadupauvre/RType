#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Key;
using engine::core::Position;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr float ship_width = 96.F;
constexpr float ship_height = 36.F;
constexpr float ship_speed = 500.F;
constexpr Vec2 ship_start{160.F, 522.F};
constexpr float max_x = 1920.F - ship_width;
constexpr float max_y = 1080.F - ship_height;
constexpr float missile_height = 24.F;
constexpr float half_cooldown = 0.125F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

InputState moving(float move_x, float move_y) {
    InputState input;
    if (move_x > 0.F) {
        input.keys.insert(Key::Right);
    } else if (move_x < 0.F) {
        input.keys.insert(Key::Left);
    }
    if (move_y > 0.F) {
        input.keys.insert(Key::Down);
    } else if (move_y < 0.F) {
        input.keys.insert(Key::Up);
    }
    return input;
}

InputState holding(std::initializer_list<Key> keys) {
    return InputState{.keys = keys, .mouse_buttons = {}, .pointer = {}};
}

InputState firing() { return holding({Key::Space}); }

class ShipTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;
    Entity _ship{0, 0};

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
        const auto ship = _runtime.spawn("ship", ship_start);
        ASSERT_TRUE(ship.has_value());
        _ship = *ship;
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

    [[nodiscard]] std::size_t missiles() { return tagged("missile").size(); }

    [[nodiscard]] Vec2 position() { return _world.get<Position>(_ship).pos; }

    void place(Vec2 at) { _world.get<Position>(_ship).pos = at; }

    void steer(float move_x, float move_y) { _runtime.set_input(_ship, moving(move_x, move_y)); }

    void hold_fire() { _runtime.set_input(_ship, firing()); }
};

} // namespace

TEST_F(ShipTest, IsDrawnWithItsSprite) {
    ASSERT_TRUE(_world.has<Sprite>(_ship));
    EXPECT_EQ(_world.get<Sprite>(_ship).path, "sprites/ship.png");
}

TEST_F(ShipTest, StaysStillWithoutInput) {
    _runtime.update(1.F);
    EXPECT_FLOAT_EQ(position().x, ship_start.x);
    EXPECT_FLOAT_EQ(position().y, ship_start.y);
}

TEST_F(ShipTest, MovesAlongBothAxesAtItsSpeed) {
    steer(1.F, -1.F);
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(position().x, ship_start.x + (ship_speed * 0.1F));
    EXPECT_FLOAT_EQ(position().y, ship_start.y - (ship_speed * 0.1F));
}

TEST_F(ShipTest, ZqsdKeysSteerLikeTheArrows) {
    _runtime.set_input(_ship, holding({Key::D, Key::Z}));
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(position().x, ship_start.x + (ship_speed * 0.1F));
    EXPECT_FLOAT_EQ(position().y, ship_start.y - (ship_speed * 0.1F));
}

TEST_F(ShipTest, OppositeKeysCancel) {
    _runtime.set_input(_ship, holding({Key::Left, Key::D, Key::Up, Key::Down}));
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(position().x, ship_start.x);
    EXPECT_FLOAT_EQ(position().y, ship_start.y);
}

TEST_F(ShipTest, IgnoresKeysWithoutABinding) {
    _runtime.set_input(_ship, holding({Key::A, Key::Enter}));
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(position().x, ship_start.x);
    EXPECT_EQ(missiles(), 0U);
}

TEST_F(ShipTest, StopsAtTheLeftAndTopEdges) {
    place({10.F, 10.F});
    steer(-1.F, -1.F);
    _runtime.update(1.F);
    EXPECT_FLOAT_EQ(position().x, 0.F);
    EXPECT_FLOAT_EQ(position().y, 0.F);
}

TEST_F(ShipTest, StopsAtTheRightAndBottomEdgesWithItsWholeBodyVisible) {
    place({max_x - 10.F, max_y - 10.F});
    steer(1.F, 1.F);
    _runtime.update(1.F);
    EXPECT_FLOAT_EQ(position().x, max_x);
    EXPECT_FLOAT_EQ(position().y, max_y);
}

TEST_F(ShipTest, DoesNotShootWithoutFire) {
    _runtime.update(1.F);
    EXPECT_EQ(missiles(), 0U);
}

TEST_F(ShipTest, FireSpawnsAMissileAtTheNoseOfTheShip) {
    hold_fire();
    _runtime.update(half_cooldown);
    const auto spawned = tagged("missile");
    ASSERT_EQ(spawned.size(), 1U);
    const Vec2 missile = _world.get<Position>(spawned.front()).pos;
    EXPECT_FLOAT_EQ(missile.x, ship_start.x + ship_width);
    EXPECT_FLOAT_EQ(missile.y, ship_start.y + ((ship_height - missile_height) / 2.F));
}

TEST_F(ShipTest, HoldingFireShootsOncePerCooldown) {
    hold_fire();
    _runtime.update(half_cooldown);
    EXPECT_EQ(missiles(), 1U);
    _runtime.update(half_cooldown);
    EXPECT_EQ(missiles(), 1U);
    _runtime.update(half_cooldown);
    EXPECT_EQ(missiles(), 2U);
}

TEST_F(ShipTest, ShootingPlaysTheLaserSoundFromTheShip) {
    hold_fire();
    _runtime.update(half_cooldown);

    EXPECT_EQ(_runtime.take_audio_commands(), (std::vector<engine::script::AudioCommand>{
                                                  {.kind = engine::script::AudioCommandKind::Sound,
                                                   .name = "laser_shoot.wav",
                                                   .source = _ship}}));
}

TEST_F(ShipTest, NotShootingPlaysNoSound) {
    _runtime.update(1.F);

    EXPECT_TRUE(_runtime.take_audio_commands().empty());
}
