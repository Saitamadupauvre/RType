#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
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
using engine::core::FieldList;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::Vec2;
using engine::physics::AABBCollider;
using engine::physics::CircleCollider;
using engine::physics::CollidesWith;
using engine::physics::PhysicsBody;

namespace {

class PhysicsFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine::physics::register_physics_fields(registry);
        world.register_component<PhysicsBody>();
        world.register_component<AABBCollider>();
        world.register_component<CircleCollider>();
        world.register_component<CollidesWith>();
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
    EXPECT_NE(registry.find_component("CollidesWith"), nullptr);
}

TEST_F(PhysicsFieldsTest, FieldsHaveExpectedKinds) {
    EXPECT_EQ(kind("PhysicsBody", "type"), FieldKind::Int);
    EXPECT_EQ(kind("PhysicsBody", "velocity"), FieldKind::Vec2);
    EXPECT_EQ(kind("PhysicsBody", "mass"), FieldKind::Float);
    EXPECT_EQ(kind("PhysicsBody", "resolve_collisions"), FieldKind::Bool);
    EXPECT_EQ(kind("PhysicsBody", "collision_mask"), FieldKind::Int);
    EXPECT_EQ(kind("AABBCollider", "size"), FieldKind::Vec2);
    EXPECT_EQ(kind("CircleCollider", "radius"), FieldKind::Float);
    EXPECT_EQ(kind("CollidesWith", "labels"), FieldKind::List);
    EXPECT_EQ(registry.find_field("CollidesWith", "labels").field->element_kind(),
              FieldKind::String);
}

TEST_F(PhysicsFieldsTest, OnlyBodyVelocityIsReplicated) {
    const ComponentDescriptor* body = registry.find_component("PhysicsBody");
    ASSERT_NE(body, nullptr);
    for (const auto& field : body->fields()) {
        EXPECT_EQ(field.flags().replicated, field.name() == "velocity") << field.name();
        EXPECT_TRUE(field.flags().editable) << field.name();
    }
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

TEST_F(PhysicsFieldsTest, ColliderFieldAddsDefaultBody) {
    registry.set(world, entity, "AABBCollider", "size", Vec2(8.0F, 4.0F));
    ASSERT_TRUE(world.has<PhysicsBody>(entity));
    EXPECT_EQ(world.get<PhysicsBody>(entity).type, PhysicsBody::Type::Static);

    const Entity round = world.spawn();
    registry.set(world, round, "CircleCollider", "radius", 2.0F);
    EXPECT_TRUE(world.has<PhysicsBody>(round));
}

TEST_F(PhysicsFieldsTest, ColliderFieldKeepsExistingBody) {
    world.add(entity, PhysicsBody{.type = PhysicsBody::Type::Dynamic});
    registry.set(world, entity, "AABBCollider", "size", Vec2(8.0F, 4.0F));
    EXPECT_EQ(world.get<PhysicsBody>(entity).type, PhysicsBody::Type::Dynamic);
}

TEST_F(PhysicsFieldsTest, RejectsNonPositiveOrNonFiniteSize) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (const Vec2 size :
         {Vec2(0.0F, 4.0F), Vec2(4.0F, -1.0F), Vec2(nan, 4.0F), Vec2(4.0F, infinity)}) {
        EXPECT_THROW(registry.set(world, entity, "AABBCollider", "size", size),
                     FieldValueOutOfRange);
    }
    EXPECT_FALSE(world.has<AABBCollider>(entity));
    EXPECT_FALSE(world.has<PhysicsBody>(entity));
}

TEST_F(PhysicsFieldsTest, SizeMessageSaysItMustBePositive) {
    try {
        registry.set(world, entity, "AABBCollider", "size", Vec2(-1.0F, 4.0F));
        FAIL() << "expected FieldValueOutOfRange";
    } catch (const FieldValueOutOfRange& error) {
        EXPECT_STREQ(error.what(), "field 'AABBCollider.size': size must be positive");
    }
}

TEST_F(PhysicsFieldsTest, RejectsNonPositiveOrNonFiniteRadius) {
    for (const float radius : {0.0F, -2.0F, std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity()}) {
        EXPECT_THROW(registry.set(world, entity, "CircleCollider", "radius", radius),
                     FieldValueOutOfRange);
    }
    EXPECT_FALSE(world.has<CircleCollider>(entity));
}

TEST_F(PhysicsFieldsTest, RejectsNonFiniteOffsets) {
    const Vec2 bad(std::numeric_limits<float>::quiet_NaN(), 0.0F);
    EXPECT_THROW(registry.set(world, entity, "AABBCollider", "offset", bad), FieldValueOutOfRange);
    EXPECT_THROW(registry.set(world, entity, "CircleCollider", "offset", bad),
                 FieldValueOutOfRange);
}

TEST_F(PhysicsFieldsTest, ReadsAndWritesCollidesWithLabels) {
    const FieldList labels{.element = FieldKind::String,
                           .items = {std::string{"enemy"}, std::string{"wall"}}};
    registry.set(world, entity, "CollidesWith", "labels", labels);
    EXPECT_TRUE(world.get<CollidesWith>(entity).tags.has("wall"));
    EXPECT_EQ(registry.get(world, entity, "CollidesWith", "labels"), FieldValue{labels});
    EXPECT_FALSE(world.has<PhysicsBody>(entity));
}
