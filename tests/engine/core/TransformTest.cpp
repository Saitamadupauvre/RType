#include <gtest/gtest.h>

#include <limits>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Transform.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldKind;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::Position;
using engine::core::Rotation;
using engine::core::rotation_of;
using engine::core::Scale;
using engine::core::scale_of;
using engine::core::Vec2;

namespace {

constexpr float not_a_number = std::numeric_limits<float>::quiet_NaN();
constexpr float infinity = std::numeric_limits<float>::infinity();

} // namespace

TEST(Transform, AccessorsReturnDefaultsWhenTypesAreNotRegistered) {
    EntityManager world;
    const Entity entity = world.spawn();
    EXPECT_EQ(rotation_of(world, entity), 0.F);
    EXPECT_EQ(scale_of(world, entity), (Vec2{1.F, 1.F}));
}

TEST(Transform, AccessorsReturnDefaultsWhenComponentsAreMissing) {
    EntityManager world;
    world.register_component<Rotation>();
    world.register_component<Scale>();
    const Entity entity = world.spawn();
    EXPECT_EQ(rotation_of(world, entity), 0.F);
    EXPECT_EQ(scale_of(world, entity), (Vec2{1.F, 1.F}));
}

TEST(Transform, AccessorsReturnStoredValues) {
    EntityManager world;
    world.register_component<Rotation>();
    world.register_component<Scale>();
    const Entity entity = world.spawn();
    world.add(entity, Rotation{.degrees = 45.F});
    world.add(entity, Scale{.factor = {-2.F, 0.5F}});
    EXPECT_EQ(rotation_of(world, entity), 45.F);
    EXPECT_EQ(scale_of(world, entity), (Vec2{-2.F, 0.5F}));
}

class TransformFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine::core::register_core_fields(registry);
        world.register_component<Position>();
        world.register_component<Rotation>();
        world.register_component<Scale>();
        entity = world.spawn();
    }

    FieldRegistry registry;
    EntityManager world;
    Entity entity{0, 0};
};

TEST_F(TransformFieldsTest, RegistersReplicatedEditableFields) {
    const FieldRef degrees = registry.find_field("Rotation", "degrees");
    ASSERT_TRUE(degrees);
    EXPECT_EQ(degrees.field->kind(), FieldKind::Float);
    EXPECT_TRUE(degrees.field->flags().replicated);
    EXPECT_TRUE(degrees.field->flags().editable);
    for (const char* axis : {"x", "y"}) {
        const FieldRef factor = registry.find_field("Scale", axis);
        ASSERT_TRUE(factor) << axis;
        EXPECT_EQ(factor.field->kind(), FieldKind::Float);
        EXPECT_TRUE(factor.field->flags().replicated);
        EXPECT_TRUE(factor.field->flags().editable);
    }
}

TEST_F(TransformFieldsTest, EntityWithoutComponentsCarriesNoField) {
    EXPECT_TRUE(registry.fields_of(world, entity).empty());
    EXPECT_EQ(registry.get(world, entity, "Rotation", "degrees"), std::nullopt);
    EXPECT_EQ(registry.get(world, entity, "Scale", "x"), std::nullopt);
}

TEST_F(TransformFieldsTest, SettingFieldsAddsComponents) {
    registry.set(world, entity, "Rotation", "degrees", 90.F);
    registry.set(world, entity, "Scale", "x", 2.F);
    registry.set(world, entity, "Scale", "y", 3.F);
    EXPECT_EQ(world.get<Rotation>(entity).degrees, 90.F);
    EXPECT_EQ(world.get<Scale>(entity).factor, (Vec2{2.F, 3.F}));
    EXPECT_EQ(registry.get(world, entity, "Rotation", "degrees"), FieldValue{90.F});
    EXPECT_EQ(registry.get(world, entity, "Scale", "x"), FieldValue{2.F});
    EXPECT_EQ(registry.get(world, entity, "Scale", "y"), FieldValue{3.F});
}

TEST_F(TransformFieldsTest, AcceptsNegativeAndZeroScale) {
    registry.set(world, entity, "Scale", "x", -1.F);
    registry.set(world, entity, "Scale", "y", 0.F);
    EXPECT_EQ(scale_of(world, entity), (Vec2{-1.F, 0.F}));
}

TEST_F(TransformFieldsTest, RejectsNonFiniteRotation) {
    for (const float degrees : {not_a_number, infinity, -infinity}) {
        EXPECT_THROW(registry.set(world, entity, "Rotation", "degrees", degrees),
                     FieldValueOutOfRange);
    }
    EXPECT_FALSE(world.has<Rotation>(entity));
}

TEST_F(TransformFieldsTest, RejectsNonFiniteScale) {
    EXPECT_THROW(registry.set(world, entity, "Scale", "x", not_a_number), FieldValueOutOfRange);
    EXPECT_THROW(registry.set(world, entity, "Scale", "y", infinity), FieldValueOutOfRange);
    EXPECT_FALSE(world.has<Scale>(entity));
}

TEST_F(TransformFieldsTest, SettingOneScaleAxisKeepsTheOtherAtOne) {
    registry.set(world, entity, "Scale", "x", 0.25F);
    EXPECT_EQ(scale_of(world, entity), (Vec2{0.25F, 1.F}));
}

TEST_F(TransformFieldsTest, RejectedValueKeepsPreviousOne) {
    registry.set(world, entity, "Rotation", "degrees", 30.F);
    EXPECT_THROW(registry.set(world, entity, "Rotation", "degrees", not_a_number),
                 FieldValueOutOfRange);
    EXPECT_EQ(rotation_of(world, entity), 30.F);
}
