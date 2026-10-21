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

constexpr float ship_size = 64.F;
constexpr float ship_speed = 500.F;
constexpr Vec2 ship_start{160.F, 508.F};
constexpr float max_x = 1920.F - ship_size;
constexpr float max_y = 1080.F - ship_size;
constexpr float missile_size = 16.F;
constexpr float half_cooldown = 0.125F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "client" / "single_player";

InputState moving(float move_x, float move_y) {
    InputState input;
    input.axes.emplace("move_x", move_x);
    input.axes.emplace("move_y", move_y);
    return input;
}

InputState firing() {
    InputState input = moving(0.F, 0.F);
    input.pressed.emplace("fire");
    return input;
}

class ShipTest : public ::testing::Test {
protected:
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
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

    [[nodiscard]] std::vector<Entity> players() { return tagged("player"); }

    [[nodiscard]] std::size_t missiles() { return tagged("missile").size(); }

    [[nodiscard]] Entity ship() {
        const auto found = players();
        if (found.size() != 1) {
            throw std::runtime_error("expected exactly one ship");
        }
        return found.front();
    }

    [[nodiscard]] Vec2 position() { return _world.get<Position>(ship()).pos; }

    void place(Vec2 at) { _world.get<Position>(ship()).pos = at; }

    void steer(float move_x, float move_y) { _runtime.set_input(ship(), moving(move_x, move_y)); }

    void hold_fire() { _runtime.set_input(ship(), firing()); }
};

} // namespace

TEST_F(ShipTest, StartSpawnsOneShipAtItsStartPosition) {
    EXPECT_EQ(players().size(), 1U);
    EXPECT_FLOAT_EQ(position().x, ship_start.x);
    EXPECT_FLOAT_EQ(position().y, ship_start.y);
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
    EXPECT_FLOAT_EQ(missile.x, ship_start.x + ship_size);
    EXPECT_FLOAT_EQ(missile.y, ship_start.y + ((ship_size - missile_size) / 2.F));
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
