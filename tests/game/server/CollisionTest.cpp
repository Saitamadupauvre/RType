#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;

namespace {

constexpr Vec2 ship_start{160.F, 522.F};
constexpr float bydo_height = 72.F;
constexpr float missile_width = 48.F;
constexpr float short_tick = 0.01F;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

class CollisionTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(game_scripts));
        ASSERT_TRUE(_runtime.spawn("collisions").has_value());
        ASSERT_TRUE(_runtime.spawn("ship", ship_start).has_value());
    }

    Entity spawn(const std::string& type, Vec2 at) {
        const auto entity = _runtime.spawn(type, at);
        if (!entity) {
            throw std::runtime_error("cannot spawn " + type);
        }
        return *entity;
    }

    void tick() {
        _runtime.update(short_tick);
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

    [[nodiscard]] bool logged_error() const {
        return std::ranges::any_of(_sink->records(), [](const auto& record) {
            return record.level >= logger::Level::Error;
        });
    }
};

} // namespace

TEST_F(CollisionTest, MissileHittingABydoDestroysBoth) {
    const Entity missile = spawn("missile", {1000.F, 300.F});
    const Entity bydo = spawn("bydo", {1000.F, 290.F});
    tick();
    EXPECT_FALSE(_world.alive(missile));
    EXPECT_FALSE(_world.alive(bydo));
}

TEST_F(CollisionTest, MissileTouchingABydoEdgeDoesNotHitIt) {
    const Entity missile = spawn("missile", {1000.F - missile_width, 300.F});
    const Entity bydo = spawn("bydo", {1000.F, 300.F});
    _runtime.update(0.F);
    _runtime.flush_destroyed();
    EXPECT_TRUE(_world.alive(missile));
    EXPECT_TRUE(_world.alive(bydo));
}

TEST_F(CollisionTest, MissileFarFromABydoDestroysNeither) {
    const Entity missile = spawn("missile", {200.F, 100.F});
    const Entity bydo = spawn("bydo", {1500.F, 800.F});
    tick();
    EXPECT_TRUE(_world.alive(missile));
    EXPECT_TRUE(_world.alive(bydo));
}

TEST_F(CollisionTest, OneMissileDestroysOnlyOneBydo) {
    spawn("missile", {1000.F, 300.F});
    spawn("bydo", {1000.F, 290.F});
    spawn("bydo", {1000.F, 300.F - (bydo_height / 2.F)});
    tick();
    EXPECT_EQ(tagged("enemy").size(), 1U);
}

TEST_F(CollisionTest, BydoHittingTheShipDestroysTheShipOnly) {
    const Entity bydo = spawn("bydo", {ship_start.x + 10.F, ship_start.y + 10.F});
    tick();
    EXPECT_TRUE(tagged("ship").empty());
    EXPECT_TRUE(_world.alive(bydo));
}

TEST_F(CollisionTest, ShipDoesNotDestroyMissiles) {
    const Entity missile = spawn("missile", {ship_start.x + 10.F, ship_start.y + 10.F});
    tick();
    EXPECT_TRUE(_world.alive(missile));
    EXPECT_EQ(tagged("ship").size(), 1U);
}

TEST_F(CollisionTest, DestroyedEntitiesNeverBreakTheNextTicks) {
    spawn("missile", {1000.F, 300.F});
    spawn("bydo", {1000.F, 290.F});
    spawn("bydo", {ship_start.x + 10.F, ship_start.y + 10.F});
    spawn("missile", {1919.F, 700.F});
    spawn("bydo", {-60.F, 900.F});
    for (int frame = 0; frame < 10; ++frame) {
        tick();
    }
    EXPECT_FALSE(logged_error());
}

TEST_F(CollisionTest, DestroyedBydoPlaysAnExplosion) {
    spawn("missile", {1000.F, 300.F});
    const Entity bydo = spawn("bydo", {1000.F, 290.F});
    tick();

    EXPECT_EQ(_runtime.take_audio_commands(), (std::vector<engine::script::AudioCommand>{
                                                  {.kind = engine::script::AudioCommandKind::Sound,
                                                   .name = "explosion.wav",
                                                   .source = bydo}}));
}

TEST_F(CollisionTest, DestroyedShipPlaysAnExplosion) {
    const Entity ship = tagged("ship").front();
    spawn("bydo", {ship_start.x + 10.F, ship_start.y + 10.F});
    tick();

    EXPECT_EQ(_runtime.take_audio_commands(), (std::vector<engine::script::AudioCommand>{
                                                  {.kind = engine::script::AudioCommandKind::Sound,
                                                   .name = "explosion.wav",
                                                   .source = ship}}));
}
