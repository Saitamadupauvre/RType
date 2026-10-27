#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string_view>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Fields.hpp"

using engine::core::ComponentDescriptor;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldKind;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::Vec2;
using engine::physics::AABBCollider;
using engine::physics::CircleCollider;
using engine::physics::PhysicsBody;

namespace {

class PhysicsFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine::physics::register_physics_fields(registry);
        world.register_component<PhysicsBody>();
        world.register_component<AABBCollider>();
        world.register_component<CircleCollider>();
    }

    [[nodiscard]] std::optional<FieldKind> kind(std::string_view component,
                                                std::string_view field) const {
        const FieldRef found = registry.find_field(component, field);
        return found ? std::optional{found.field->kind()} : std::nullopt;
    }

    FieldRegistry registry;
    EntityManager world;
    Entity entity = world.spawn();
};

} // namespace

TEST_F(PhysicsFieldsTest, RegistersEveryPhysicsComponent) {
    const ComponentDescriptor* body = registry.find_component("PhysicsBody");
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->fields().size(), 8U);
    EXPECT_NE(registry.find_component("AABBCollider"), nullptr);
    EXPECT_NE(registry.find_component("CircleCollider"), nullptr);
}

TEST_F(PhysicsFieldsTest, FieldsHaveExpectedKinds) {
    EXPECT_EQ(kind("PhysicsBody", "type"), FieldKind::Int);
    EXPECT_EQ(kind("PhysicsBody", "velocity"), FieldKind::Vec2);
    EXPECT_EQ(kind("PhysicsBody", "mass"), FieldKind::Float);
    EXPECT_EQ(kind("PhysicsBody", "resolve_collisions"), FieldKind::Bool);
    EXPECT_EQ(kind("PhysicsBody", "collision_mask"), FieldKind::Int);
    EXPECT_EQ(kind("AABBCollider", "size"), FieldKind::Vec2);
    EXPECT_EQ(kind("CircleCollider", "radius"), FieldKind::Float);
}

TEST_F(PhysicsFieldsTest, ReadsAndWritesBody) {
    registry.set(world, entity, "PhysicsBody", "type", std::int64_t{1});
    registry.set(world, entity, "PhysicsBody", "mass", 2.5F);
    registry.set(world, entity, "PhysicsBody", "affected_by_gravity", false);
    registry.set(world, entity, "PhysicsBody", "collision_mask", std::int64_t{0xFFFFFFFF});

    const PhysicsBody& body = world.get<PhysicsBody>(entity);
    EXPECT_EQ(body.type, PhysicsBody::Type::Dynamic);
    EXPECT_FLOAT_EQ(body.mass, 2.5F);
    EXPECT_FALSE(body.affected_by_gravity);
    EXPECT_EQ(body.collision_mask, 0xFFFFFFFFU);
    EXPECT_EQ(registry.get(world, entity, "PhysicsBody", "type"), FieldValue{std::int64_t{1}});
}

TEST_F(PhysicsFieldsTest, RejectsUnknownBodyType) {
    EXPECT_THROW(registry.set(world, entity, "PhysicsBody", "type", std::int64_t{2}),
                 FieldValueOutOfRange);
    EXPECT_THROW(registry.set(world, entity, "PhysicsBody", "type", std::int64_t{-1}),
                 FieldValueOutOfRange);
    EXPECT_FALSE(world.has<PhysicsBody>(entity));
}

TEST_F(PhysicsFieldsTest, RejectsCollisionLayerOutOfRange) {
    EXPECT_THROW(
        registry.set(world, entity, "PhysicsBody", "collision_layer", std::int64_t{0x100000000}),
        FieldValueOutOfRange);
}

TEST_F(PhysicsFieldsTest, ReadsAndWritesColliders) {
    registry.set(world, entity, "AABBCollider", "size", Vec2(8.0F, 4.0F));
    registry.set(world, entity, "CircleCollider", "radius", 3.0F);
    EXPECT_EQ(world.get<AABBCollider>(entity).size, Vec2(8.0F, 4.0F));
    EXPECT_FLOAT_EQ(world.get<CircleCollider>(entity).radius, 3.0F);
    EXPECT_EQ(registry.get(world, entity, "CircleCollider", "offset"), FieldValue{Vec2{}});
}

TEST_F(PhysicsFieldsTest, RegisteringTwiceThrows) {
    EXPECT_THROW(engine::physics::register_physics_fields(registry),
                 engine::core::DuplicateComponentName);
}
