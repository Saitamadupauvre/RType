#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/physics/Components.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Key;
using engine::core::MouseButton;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;
using engine::physics::PhysicsBody;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path tick_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "tick";
}

class TickTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(tick_fixture()));
    }

    Entity spawn(const std::string& type) {
        const auto entity = _runtime.spawn(type);
        if (!entity) {
            throw std::runtime_error("spawn failed: " + type);
        }
        return *entity;
    }

    [[nodiscard]] Vec2 position(Entity entity) { return _world.get<Position>(entity).pos; }

    [[nodiscard]] int count_tagged(const std::string& label) {
        int count = 0;
        _world.query<Tags>().each(
            [&](Entity, const Tags& tags) { count += tags.has(label) ? 1 : 0; });
        return count;
    }

    [[nodiscard]] int count_y(float value) {
        int count = 0;
        _world.query<Position>().each(
            [&](Entity, const Position& position) { count += position.pos.y == value ? 1 : 0; });
        return count;
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(TickTest, UpdatePassesDtToEntities) {
    const Entity mover = spawn("mover");
    _runtime.update(0.5F);
    EXPECT_FLOAT_EQ(position(mover).x, 5.0F);
}

TEST_F(TickTest, AgeAdvancesByDtEachUpdate) {
    const Entity mover = spawn("mover");
    _runtime.update(0.25F);
    _runtime.update(0.25F);
    EXPECT_FLOAT_EQ(position(mover).y, 0.5F);
}

TEST_F(TickTest, AgeIsReadOnly) {
    EXPECT_FALSE(_runtime.run_string("spawn('mover').age = 3", "test"));
    EXPECT_TRUE(logged_error("read-only"));
}

TEST_F(TickTest, OnSpawnCalledForNativeSpawn) {
    const Entity greeter = spawn("greeter");
    EXPECT_FLOAT_EQ(position(greeter).y, 42.0F);
}

TEST_F(TickTest, OnSpawnCalledForLuaSpawn) {
    spawn("parent");
    _runtime.update(0.1F);
    EXPECT_EQ(count_y(42.0F), 1);
}

TEST_F(TickTest, EntitySpawnedDuringUpdateWaitsForNextTick) {
    spawn("parent");
    _runtime.update(0.1F);
    EXPECT_EQ(count_y(42.0F), 1);
    _runtime.update(0.1F);
    EXPECT_EQ(count_y(42.0F), 2);
}

TEST_F(TickTest, DestroyIsDeferredUntilFlush) {
    const Entity doomed = spawn("doomed");
    _runtime.update(0.1F);
    ASSERT_TRUE(_world.alive(doomed));
    EXPECT_FLOAT_EQ(position(doomed).x, 7.0F);

    EXPECT_EQ(_runtime.flush_destroyed(), 1U);
    EXPECT_FALSE(_world.alive(doomed));
}

TEST_F(TickTest, FlushCallsOnDestroyOnceEvenIfDestroyedTwice) {
    spawn("doomed");
    _runtime.update(0.1F);
    _runtime.flush_destroyed();
    EXPECT_EQ(count_tagged("marker"), 1);
}

TEST_F(TickTest, FlushWithNothingMarkedDoesNothing) {
    const Entity mover = spawn("mover");
    EXPECT_EQ(_runtime.flush_destroyed(), 0U);
    EXPECT_TRUE(_world.alive(mover));
}

TEST_F(TickTest, EntityDestroyedInOnDestroyIsFlushedInSameCall) {
    const Entity chain = spawn("chain");
    ASSERT_TRUE(_runtime.call(chain, "die"));
    EXPECT_EQ(_runtime.flush_destroyed(), 2U);
    EXPECT_EQ(count_tagged("marker"), 1);
}

TEST_F(TickTest, MarkedEntityIsNotUpdated) {
    ASSERT_TRUE(_runtime.run_string("spawn('mover'):destroy()", "mark"));
    const Entity mover = spawn("mover");
    _runtime.update(1.0F);
    int moved = 0;
    _world.query<Position>().each(
        [&](Entity, const Position& position) { moved += position.pos.x > 0.0F ? 1 : 0; });
    EXPECT_EQ(moved, 1);
    EXPECT_FLOAT_EQ(position(mover).x, 10.0F);
}

TEST_F(TickTest, ErrorInUpdateIsLoggedAndOthersStillRun) {
    spawn("faulty");
    const Entity mover = spawn("mover");
    _runtime.update(1.0F);
    EXPECT_TRUE(logged_error("entities.lua:"));
    EXPECT_TRUE(logged_error("faulty update"));
    EXPECT_FLOAT_EQ(position(mover).x, 10.0F);
}

TEST_F(TickTest, ErrorKeepsTheEntityRunning) {
    const Entity faulty = spawn("faulty");
    _runtime.update(1.0F);
    _runtime.update(1.0F);
    EXPECT_TRUE(_world.alive(faulty));
}

TEST_F(TickTest, SpriteAndTagsComeFromPrefab) {
    const Entity ship = spawn("ship");
    EXPECT_EQ(_world.get<Sprite>(ship).path, "ship.png");
    EXPECT_EQ(_world.get<Tags>(ship).labels(), (std::vector<std::string>{"player", "ally"}));
}

TEST_F(TickTest, TagsReadBackAsLuaArray) {
    const Entity ship = spawn("ship");
    ASSERT_TRUE(_runtime.call(ship, "count_tags"));
    EXPECT_FLOAT_EQ(position(ship).x, 2.0F);
}

TEST_F(TickTest, WritingSpriteAndTagsUpdatesComponents) {
    const Entity ship = spawn("ship");
    ASSERT_TRUE(_runtime.call(ship, "retag"));
    EXPECT_EQ(_world.get<Sprite>(ship).path, "bydo.png");
    EXPECT_EQ(_world.get<Tags>(ship).labels(), (std::vector<std::string>{"enemy"}));
}

TEST_F(TickTest, ColorFromPrefabDefaultsAlphaAndHasNoImage) {
    const Entity painted = spawn("painted");
    const Sprite& sprite = _world.get<Sprite>(painted);
    EXPECT_EQ(sprite.color, (Rgba{.r = 10, .g = 20, .b = 30, .a = 255}));
    EXPECT_TRUE(sprite.path.empty());
}

TEST_F(TickTest, WritingColorUpdatesSprite) {
    const Entity painted = spawn("painted");
    ASSERT_TRUE(_runtime.call(painted, "repaint"));
    EXPECT_EQ(_world.get<Sprite>(painted).color, (Rgba{.r = 255, .g = 0, .b = 0, .a = 128}));
}

TEST_F(TickTest, ColorReadsBackAsTable) {
    const Entity painted = spawn("painted");
    ASSERT_TRUE(_runtime.call(painted, "read_color"));
    EXPECT_FLOAT_EQ(position(painted).x, 20.0F);
    EXPECT_FLOAT_EQ(position(painted).y, 255.0F);
}

TEST_F(TickTest, SpriteWithoutColorIsWhite) {
    const Entity ship = spawn("ship");
    EXPECT_EQ(_world.get<Sprite>(ship).path, "ship.png");
    EXPECT_EQ(_world.get<Sprite>(ship).color, Rgba{});
}

TEST_F(TickTest, ColorOutOfRangeIsRejected) {
    const Entity painted = spawn("painted");
    EXPECT_FALSE(_runtime.call(painted, "out_of_range"));
    EXPECT_TRUE(logged_error("must be in [0, 255]"));
    EXPECT_EQ(_world.get<Sprite>(painted).color.r, 10);
}

TEST_F(TickTest, ColorMissingChannelIsRejected) {
    const Entity painted = spawn("painted");
    EXPECT_FALSE(_runtime.call(painted, "missing_channel"));
    EXPECT_TRUE(logged_error("needs a number 'b'"));
}

TEST_F(TickTest, NonStringTagFailsSpawn) {
    EXPECT_FALSE(_runtime.spawn("bad_tags").has_value());
    EXPECT_TRUE(logged_error("must only hold strings"));
}

TEST_F(TickTest, WrongTypeForSpriteIsRejected) {
    EXPECT_FALSE(_runtime.run_string("spawn('ship', { sprite = 3 })", "test"));
    EXPECT_TRUE(logged_error("'sprite' cannot be set to a number"));
}

TEST_F(TickTest, HasTagReadsTags) {
    const Entity ship = spawn("ship");
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(position(ship).y, 1.0F);
}

TEST_F(TickTest, InputDrivesEntity) {
    const Entity ship = spawn("ship");
    _server.set_input(
        ship,
        InputState{.keys = {Key::Right, Key::Up, Key::Space}, .mouse_buttons = {}, .pointer = {}});
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(_world.get<PhysicsBody>(ship).velocity.x, 100.0F);
    EXPECT_FLOAT_EQ(_world.get<PhysicsBody>(ship).velocity.y, -100.0F);
    EXPECT_FLOAT_EQ(position(ship).x, 5.0F);
}

TEST_F(TickTest, InputGivesPointerAndMouseButtons) {
    const Entity cursor = spawn("cursor");
    _server.set_input(
        cursor,
        InputState{.keys = {}, .mouse_buttons = {MouseButton::Left}, .pointer = Vec2{120.F, 45.F}});
    _runtime.update(0.1F);
    EXPECT_EQ(position(cursor), (Vec2{120.F, 45.F}));
    ASSERT_TRUE(_runtime.call(cursor, "read_click"));
    EXPECT_FLOAT_EQ(position(cursor).x, 1.F);
}

TEST_F(TickTest, PointerIsNilWithoutOne) {
    const Entity cursor = spawn("cursor");
    _runtime.update(0.1F);
    EXPECT_EQ(position(cursor), (Vec2{-1.F, -1.F}));
    ASSERT_TRUE(_runtime.call(cursor, "read_click"));
    EXPECT_FLOAT_EQ(position(cursor).x, 0.F);
}

TEST_F(TickTest, UnknownKeyNameIsALuaError) {
    const Entity cursor = spawn("cursor");
    EXPECT_FALSE(_runtime.call(cursor, "read_unknown_key"));
    EXPECT_TRUE(logged_error("input:key: unknown key 'Spcae'"));
}

TEST_F(TickTest, UnknownMouseButtonNameIsALuaError) {
    const Entity cursor = spawn("cursor");
    EXPECT_FALSE(_runtime.call(cursor, "read_unknown_button"));
    EXPECT_TRUE(logged_error("input:mouse: unknown mouse button 'Back'"));
}

TEST_F(TickTest, KeyNameMustBeAString) {
    const Entity cursor = spawn("cursor");
    EXPECT_FALSE(_runtime.call(cursor, "read_key_of_wrong_type"));
    EXPECT_TRUE(logged_error("input:key: unknown key of type number"));
}

TEST_F(TickTest, MissingInputIsNeutral) {
    const Entity ship = spawn("ship");
    _runtime.update(0.1F);
    EXPECT_FLOAT_EQ(_world.get<PhysicsBody>(ship).velocity.x, 0.0F);
    EXPECT_FLOAT_EQ(position(ship).x, 0.0F);
}

TEST_F(TickTest, InputIsReadOnly) {
    const Entity ship = spawn("ship");
    EXPECT_FALSE(_runtime.call(ship, "break_input"));
    EXPECT_TRUE(logged_error("read-only"));
}

TEST_F(TickTest, SetInputOnUnknownEntityIsIgnored) {
    EXPECT_NO_THROW(_server.set_input(Entity{999}, InputState{}));
}

TEST_F(TickTest, HandleIsStaleAfterFlush) {
    ASSERT_TRUE(_runtime.run_string("stale = spawn('mover') stale:destroy()", "mark"));
    _runtime.flush_destroyed();
    EXPECT_FALSE(_runtime.run_string("stale:destroy()", "late"));
    EXPECT_TRUE(logged_error("destroyed entity"));
}
