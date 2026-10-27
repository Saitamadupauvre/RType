#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/physics/Components.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldKind;
using engine::core::Position;
using engine::core::Vec2;
using engine::physics::AABBCollider;
using engine::physics::PhysicsBody;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path entities_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "entities";
}

class EntityFieldsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(entities_fixture()));
        ASSERT_TRUE(_runtime.run_string("bydo = spawn('bydo')", "setup"));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] Entity bydo() {
        std::optional<Entity> found;
        _world.query<Position>().each([&](Entity entity, Position&) { found = entity; });
        EXPECT_TRUE(found.has_value());
        return found.value_or(Entity{0});
    }
};

} // namespace

TEST_F(EntityFieldsTest, RuntimeSharesItsFieldRegistry) {
    const engine::core::FieldRegistry& fields = _runtime.fields();
    EXPECT_TRUE(fields.find_field("Position", "pos"));
    EXPECT_TRUE(fields.find_field("PhysicsBody", "mass"));
    const engine::core::FieldRef labels = fields.find_field("CollidesWith", "labels");
    ASSERT_TRUE(labels);
    EXPECT_EQ(labels.field->kind(), FieldKind::List);
}

TEST_F(EntityFieldsTest, SetFieldWritesAnyRegisteredField) {
    ASSERT_TRUE(_runtime.run_string("bydo:set_field('PhysicsBody', 'mass', 2.5)", "test"));
    ASSERT_TRUE(_world.has<PhysicsBody>(bydo()));
    EXPECT_FLOAT_EQ(_world.get<PhysicsBody>(bydo()).mass, 2.5F);
    EXPECT_TRUE(
        _runtime.run_string("assert(bydo:get_field('PhysicsBody', 'mass') == 2.5)", "test"));
}

TEST_F(EntityFieldsTest, GetFieldIsNilWithoutComponent) {
    EXPECT_TRUE(
        _runtime.run_string("assert(bydo:get_field('CircleCollider', 'radius') == nil)", "test"));
}

TEST_F(EntityFieldsTest, Vec2FieldsAreTablesWithXAndY) {
    ASSERT_TRUE(_runtime.run_string("bydo:set_field('Position', 'pos', { x = 3, y = 4 })", "test"));
    EXPECT_EQ(_world.get<Position>(bydo()).pos, Vec2(3.0F, 4.0F));
    EXPECT_TRUE(_runtime.run_string(R"(
        local pos = bydo:get_field('Position', 'pos')
        assert(pos.x == 3 and pos.y == 4 and bydo.x == 3)
    )",
                                    "test"));
}

TEST_F(EntityFieldsTest, ListFieldsAreArrays) {
    ASSERT_TRUE(
        _runtime.run_string("bydo:set_field('Tags', 'labels', { 'enemy', 'boss' })", "test"));
    EXPECT_TRUE(_world.get<engine::core::Tags>(bydo()).has("boss"));
    EXPECT_TRUE(_runtime.run_string(R"(
        local labels = bydo:get_field('Tags', 'labels')
        assert(#labels == 2 and labels[1] == 'enemy' and bydo:has_tag('boss'))
    )",
                                    "test"));
}

TEST_F(EntityFieldsTest, ListAliasIsEmptyWithoutComponent) {
    EXPECT_TRUE(_runtime.run_string("assert(#bydo.collides_with == 0)", "test"));
}

TEST_F(EntityFieldsTest, UnknownFieldIsALuaErrorWithLocation) {
    EXPECT_FALSE(_runtime.run_string("bydo:get_field('Ghost', 'mass')", "test"));
    EXPECT_TRUE(logged_error("test:1: unknown entity field 'Ghost.mass'"));
    EXPECT_FALSE(_runtime.run_string("bydo:set_field('PhysicsBody', 'ghost', 1)", "test"));
    EXPECT_TRUE(logged_error("test:1: unknown entity field 'PhysicsBody.ghost'"));
}

TEST_F(EntityFieldsTest, WrongLuaTypeNamesTheField) {
    EXPECT_FALSE(_runtime.run_string("bydo:set_field('PhysicsBody', 'mass', 'heavy')", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'PhysicsBody.mass' cannot be set to a string"));
    EXPECT_FALSE(_world.has<PhysicsBody>(bydo()));
}

TEST_F(EntityFieldsTest, RejectsNonFiniteNumbers) {
    EXPECT_FALSE(_runtime.run_string("bydo.x = 0 / 0", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'x' must be a finite number"));
    EXPECT_FALSE(_runtime.run_string("bydo.vy = math.huge", "test"));
    EXPECT_TRUE(logged_error("entity field 'vy' must be a finite number"));
    EXPECT_FALSE(
        _runtime.run_string("bydo:set_field('Position', 'pos', { x = 1, y = 1e39 })", "test"));
    EXPECT_FLOAT_EQ(_world.get<Position>(bydo()).pos.x, 800.0F);
}

TEST_F(EntityFieldsTest, IntegerFieldsNeedWholeNumbers) {
    EXPECT_FALSE(
        _runtime.run_string("bydo:set_field('PhysicsBody', 'collision_layer', 1.5)", "test"));
    EXPECT_TRUE(logged_error("entity field 'PhysicsBody.collision_layer' must be a whole number"));
    EXPECT_TRUE(
        _runtime.run_string("bydo:set_field('PhysicsBody', 'collision_layer', 4.0)", "test"));
    EXPECT_EQ(_world.get<PhysicsBody>(bydo()).collision_layer, 4U);
}

TEST_F(EntityFieldsTest, OutOfRangeValueReportsTheComponentReason) {
    EXPECT_FALSE(_runtime.run_string("bydo:set_field('PhysicsBody', 'type', 5)", "test"));
    EXPECT_TRUE(logged_error(
        "test:1: entity field 'PhysicsBody.type': body type must be 0 (static) or 1 (dynamic)"));
    EXPECT_FALSE(_world.has<PhysicsBody>(bydo()));
}

TEST_F(EntityFieldsTest, RejectedHitboxAddsNoComponent) {
    EXPECT_FALSE(_runtime.run_string("bydo.hitbox = { w = -1, h = 4 }", "test"));
    EXPECT_TRUE(logged_error("entity field 'hitbox': size must be positive"));
    EXPECT_FALSE(_world.has<AABBCollider>(bydo()));
    EXPECT_FALSE(_world.has<PhysicsBody>(bydo()));
    EXPECT_FALSE(_world.has<engine::physics::Scriptable>(bydo()));
}

TEST_F(EntityFieldsTest, HitboxGivesAScriptBodyCentredCollider) {
    ASSERT_TRUE(_runtime.run_string("bydo.hitbox = { w = 8, h = 4 }", "test"));
    const PhysicsBody& body = _world.get<PhysicsBody>(bydo());
    EXPECT_EQ(body.type, PhysicsBody::Type::Dynamic);
    EXPECT_FALSE(body.affected_by_gravity);
    EXPECT_FALSE(body.resolve_collisions);
    EXPECT_EQ(_world.get<AABBCollider>(bydo()).offset, Vec2(4.0F, 2.0F));
    EXPECT_TRUE(_world.has<engine::physics::Scriptable>(bydo()));
    EXPECT_TRUE(_runtime.run_string("assert(bydo.hitbox.w == 8 and bydo.hitbox.h == 4)", "test"));
}

TEST_F(EntityFieldsTest, HitboxKeepsAnExistingBody) {
    ASSERT_TRUE(_runtime.run_string("bydo:set_field('PhysicsBody', 'mass', 3)", "test"));
    ASSERT_TRUE(_runtime.run_string("bydo.hitbox = { w = 8, h = 4 }", "test"));
    EXPECT_EQ(_world.get<PhysicsBody>(bydo()).type, PhysicsBody::Type::Static);
    EXPECT_FLOAT_EQ(_world.get<PhysicsBody>(bydo()).mass, 3.0F);
}

TEST_F(EntityFieldsTest, SpriteIsNilUntilSet) {
    EXPECT_TRUE(_runtime.run_string("assert(bydo.sprite == nil and bydo.color == nil)", "test"));
    EXPECT_TRUE(_runtime.run_string(R"(
        bydo.sprite = 'bydo.png'
        assert(bydo.sprite == 'bydo.png' and bydo.color.a == 255)
    )",
                                    "test"));
}

TEST_F(EntityFieldsTest, FieldMethodsNeedALiveEntity) {
    ASSERT_TRUE(_runtime.run_string("bydo:destroy()", "test"));
    _runtime.flush_destroyed();
    EXPECT_FALSE(_runtime.run_string("bydo:get_field('Position', 'pos')", "stale"));
    EXPECT_TRUE(logged_error("stale:1: attempt to use a destroyed entity"));
}
