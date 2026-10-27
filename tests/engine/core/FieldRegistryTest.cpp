#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"

using engine::core::AssetName;
using engine::core::ComponentDescriptor;
using engine::core::DuplicateComponentName;
using engine::core::DuplicateFieldName;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldDescriptor;
using engine::core::FieldFlags;
using engine::core::FieldKind;
using engine::core::FieldKindMismatch;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::member_field;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Sprite;
using engine::core::UnknownField;
using engine::core::Vec2;
using engine::core::Velocity;

namespace {

struct Sample {
    float speed = 1.5F;
    std::int64_t score = 7;
    bool alive = true;
    std::string label = "ship";
    Vec2 offset{1.0F, 2.0F};
    Rgba tint{.r = 10, .g = 20, .b = 30, .a = 40};
    std::string icon = "icon.png";
    std::uint8_t lives = 3;
};

FieldRegistry sample_registry() {
    FieldRegistry registry;
    registry.add<Sample>(
        "Sample",
        {member_field<&Sample::speed>("speed"), member_field<&Sample::score>("score"),
         member_field<&Sample::alive>("alive"), member_field<&Sample::label>("label"),
         member_field<&Sample::offset>("offset"), member_field<&Sample::tint>("tint"),
         engine::core::asset_field<&Sample::icon>("icon"), member_field<&Sample::lives>("lives")});
    return registry;
}

class FieldRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        world.register_component<Sample>();
        world.register_component<Position>();
        world.register_component<Velocity>();
        world.register_component<Sprite>();
    }

    EntityManager world;
    Entity entity = world.spawn();
    FieldRegistry registry = sample_registry();
};

} // namespace

TEST(FieldKind, NamesEveryKind) {
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Float), "float");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Int), "int");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Bool), "bool");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::String), "string");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Vec2), "vec2");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Color), "color");
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::Asset), "asset");
}

TEST(FieldKind, InvalidKindIsUnknown) {
    EXPECT_EQ(engine::core::field_kind_name(static_cast<FieldKind>(200)), "unknown");
}

TEST(FieldKind, KindOfMatchesHeldValue) {
    EXPECT_EQ(engine::core::kind_of(FieldValue{1.0F}), FieldKind::Float);
    EXPECT_EQ(engine::core::kind_of(FieldValue{std::int64_t{1}}), FieldKind::Int);
    EXPECT_EQ(engine::core::kind_of(FieldValue{true}), FieldKind::Bool);
    EXPECT_EQ(engine::core::kind_of(FieldValue{std::string{"a"}}), FieldKind::String);
    EXPECT_EQ(engine::core::kind_of(FieldValue{Vec2{}}), FieldKind::Vec2);
    EXPECT_EQ(engine::core::kind_of(FieldValue{Rgba{}}), FieldKind::Color);
    EXPECT_EQ(engine::core::kind_of(FieldValue{AssetName{"a.png"}}), FieldKind::Asset);
}

TEST_F(FieldRegistryTest, DeducesKindFromMemberType) {
    const ComponentDescriptor* sample = registry.find_component("Sample");
    ASSERT_NE(sample, nullptr);
    const std::vector<FieldKind> expected{FieldKind::Float,  FieldKind::Int,  FieldKind::Bool,
                                          FieldKind::String, FieldKind::Vec2, FieldKind::Color,
                                          FieldKind::Asset,  FieldKind::Int};
    ASSERT_EQ(sample->fields().size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(sample->fields()[index].kind(), expected[index])
            << sample->fields()[index].name();
    }
}

TEST_F(FieldRegistryTest, GetsEveryKind) {
    world.add(entity, Sample{});
    EXPECT_EQ(registry.get(world, entity, "Sample", "speed"), FieldValue{1.5F});
    EXPECT_EQ(registry.get(world, entity, "Sample", "score"), FieldValue{std::int64_t{7}});
    EXPECT_EQ(registry.get(world, entity, "Sample", "alive"), FieldValue{true});
    EXPECT_EQ(registry.get(world, entity, "Sample", "label"), FieldValue{std::string{"ship"}});
    EXPECT_EQ(registry.get(world, entity, "Sample", "offset"), FieldValue{Vec2(1.0F, 2.0F)});
    EXPECT_EQ(registry.get(world, entity, "Sample", "tint"),
              FieldValue{(Rgba{.r = 10, .g = 20, .b = 30, .a = 40})});
    EXPECT_EQ(registry.get(world, entity, "Sample", "icon"), FieldValue{AssetName{"icon.png"}});
    EXPECT_EQ(registry.get(world, entity, "Sample", "lives"), FieldValue{std::int64_t{3}});
}

TEST_F(FieldRegistryTest, SetsEveryKind) {
    world.add(entity, Sample{});
    registry.set(world, entity, "Sample", "speed", 4.0F);
    registry.set(world, entity, "Sample", "score", std::int64_t{-12});
    registry.set(world, entity, "Sample", "alive", false);
    registry.set(world, entity, "Sample", "label", std::string{"boss"});
    registry.set(world, entity, "Sample", "offset", Vec2(5.0F, 6.0F));
    registry.set(world, entity, "Sample", "tint", Rgba{.r = 1, .g = 2, .b = 3, .a = 4});
    registry.set(world, entity, "Sample", "icon", AssetName{"boss.png"});
    registry.set(world, entity, "Sample", "lives", std::int64_t{9});

    const Sample& sample = world.get<Sample>(entity);
    EXPECT_FLOAT_EQ(sample.speed, 4.0F);
    EXPECT_EQ(sample.score, -12);
    EXPECT_FALSE(sample.alive);
    EXPECT_EQ(sample.label, "boss");
    EXPECT_EQ(sample.offset, Vec2(5.0F, 6.0F));
    EXPECT_EQ(sample.tint, (Rgba{.r = 1, .g = 2, .b = 3, .a = 4}));
    EXPECT_EQ(sample.icon, "boss.png");
    EXPECT_EQ(sample.lives, 9);
}

TEST_F(FieldRegistryTest, ReadsAndWritesThroughFieldRef) {
    const FieldRef speed = registry.find_field("Sample", "speed");
    ASSERT_TRUE(speed);
    registry.set(world, entity, speed, 2.0F);
    EXPECT_EQ(registry.get(world, entity, speed), FieldValue{2.0F});
}

TEST_F(FieldRegistryTest, FieldRefStillChecksKind) {
    const FieldRef speed = registry.find_field("Sample", "speed");
    EXPECT_THROW(registry.set(world, entity, speed, true), FieldKindMismatch);
    EXPECT_FALSE(world.has<Sample>(entity));
}

TEST_F(FieldRegistryTest, EmptyFieldRefThrows) {
    EXPECT_THROW((void)registry.get(world, entity, FieldRef{}), std::invalid_argument);
    EXPECT_THROW(registry.set(world, entity, FieldRef{}, 1.0F), std::invalid_argument);
}

TEST_F(FieldRegistryTest, GetReturnsNulloptWithoutComponent) {
    EXPECT_EQ(registry.get(world, entity, "Sample", "speed"), std::nullopt);
}

TEST_F(FieldRegistryTest, SetAddsMissingComponentWithDefaults) {
    registry.set(world, entity, "Sample", "speed", 9.0F);
    ASSERT_TRUE(world.has<Sample>(entity));
    EXPECT_FLOAT_EQ(world.get<Sample>(entity).speed, 9.0F);
    EXPECT_EQ(world.get<Sample>(entity).label, "ship");
}

TEST_F(FieldRegistryTest, RejectsWrongKind) {
    world.add(entity, Sample{});
    EXPECT_THROW(registry.set(world, entity, "Sample", "speed", std::string{"fast"}),
                 FieldKindMismatch);
    EXPECT_THROW(registry.set(world, entity, "Sample", "icon", std::string{"a.png"}),
                 FieldKindMismatch);
    EXPECT_THROW(registry.set(world, entity, "Sample", "score", 1.0F), FieldKindMismatch);
    EXPECT_FLOAT_EQ(world.get<Sample>(entity).speed, 1.5F);
    EXPECT_EQ(world.get<Sample>(entity).icon, "icon.png");
}

TEST_F(FieldRegistryTest, WrongKindDoesNotAddComponent) {
    EXPECT_THROW(registry.set(world, entity, "Sample", "alive", 1.0F), FieldKindMismatch);
    EXPECT_FALSE(world.has<Sample>(entity));
}

TEST_F(FieldRegistryTest, WrongKindMessageNamesFieldAndKinds) {
    try {
        registry.set(world, entity, "Sample", "speed", true);
        FAIL() << "expected FieldKindMismatch";
    } catch (const FieldKindMismatch& error) {
        EXPECT_STREQ(error.what(), "field 'Sample.speed' expects a float, not a bool");
    }
}

TEST_F(FieldRegistryTest, RejectsIntegerOutOfMemberRange) {
    world.add(entity, Sample{});
    EXPECT_THROW(registry.set(world, entity, "Sample", "lives", std::int64_t{256}),
                 FieldValueOutOfRange);
    EXPECT_THROW(registry.set(world, entity, "Sample", "lives", std::int64_t{-1}),
                 FieldValueOutOfRange);
    EXPECT_EQ(world.get<Sample>(entity).lives, 3);
}

TEST_F(FieldRegistryTest, OutOfRangeDoesNotAddComponent) {
    EXPECT_THROW(
        registry.set(world, entity, "Sample", "lives", std::numeric_limits<std::int64_t>::max()),
        FieldValueOutOfRange);
    EXPECT_FALSE(world.has<Sample>(entity));
}

TEST_F(FieldRegistryTest, OutOfRangeMessageNamesField) {
    try {
        registry.set(world, entity, "Sample", "lives", std::int64_t{256});
        FAIL() << "expected FieldValueOutOfRange";
    } catch (const FieldValueOutOfRange& error) {
        EXPECT_STREQ(error.what(), "field 'Sample.lives': integer does not fit the member");
    }
}

TEST_F(FieldRegistryTest, UnknownComponentOrFieldThrows) {
    EXPECT_THROW((void)registry.get(world, entity, "Ghost", "speed"), UnknownField);
    EXPECT_THROW((void)registry.get(world, entity, "Sample", "ghost"), UnknownField);
    EXPECT_THROW(registry.set(world, entity, "Ghost", "speed", 1.0F), UnknownField);
    EXPECT_THROW(registry.set(world, entity, "Sample", "ghost", 1.0F), UnknownField);
}

TEST_F(FieldRegistryTest, SetOnDeadEntityThrows) {
    world.kill(entity);
    EXPECT_THROW(registry.set(world, entity, "Sample", "speed", 1.0F), std::invalid_argument);
}

TEST_F(FieldRegistryTest, FindsComponentsAndFields) {
    EXPECT_NE(registry.find_component("Sample"), nullptr);
    EXPECT_EQ(registry.find_component("Ghost"), nullptr);
    const FieldRef speed = registry.find_field("Sample", "speed");
    ASSERT_TRUE(speed);
    EXPECT_EQ(speed.component->name(), "Sample");
    EXPECT_EQ(speed.field->name(), "speed");
    EXPECT_FALSE(registry.find_field("Sample", "ghost"));
    EXPECT_FALSE(registry.find_field("Ghost", "speed"));
}

TEST(FieldRegistry, RejectsDuplicateComponentName) {
    FieldRegistry registry = sample_registry();
    EXPECT_THROW(registry.add<Position>("Sample", {member_field<&Position::pos>("pos")}),
                 DuplicateComponentName);
    EXPECT_EQ(registry.components().size(), 1U);
}

TEST(FieldRegistry, RejectsSameTypeUnderAnotherName) {
    FieldRegistry registry = sample_registry();
    EXPECT_THROW(registry.add<Sample>("Other", {member_field<&Sample::speed>("speed")}),
                 DuplicateComponentName);
    EXPECT_EQ(registry.find_component("Other"), nullptr);
}

TEST(FieldRegistry, RejectsDuplicateFieldName) {
    FieldRegistry registry;
    EXPECT_THROW(registry.add<Sample>("Sample", {member_field<&Sample::speed>("speed"),
                                                 member_field<&Sample::score>("speed")}),
                 DuplicateFieldName);
    EXPECT_EQ(registry.find_component("Sample"), nullptr);
}

TEST(FieldRegistry, RejectsEmptyNames) {
    FieldRegistry registry;
    EXPECT_THROW(registry.add<Sample>("", {member_field<&Sample::speed>("speed")}),
                 std::invalid_argument);
    EXPECT_THROW(registry.add<Sample>("Sample", {member_field<&Sample::speed>("")}),
                 std::invalid_argument);
    EXPECT_TRUE(registry.components().empty());
}

TEST(FieldRegistry, RejectsMissingAccessors) {
    FieldRegistry registry;
    EXPECT_THROW(registry.add<Sample>(
                     "Sample", {FieldDescriptor{"speed", FieldKind::Float, {}, nullptr, nullptr}}),
                 std::invalid_argument);
    EXPECT_TRUE(registry.components().empty());
}

TEST_F(FieldRegistryTest, FieldsOfListsOnlyOwnedComponents) {
    engine::core::register_core_fields(registry);
    world.add(entity, Position{1.0F, 2.0F});
    world.add(entity, Sprite{});

    std::vector<std::string> names;
    for (const FieldRef& ref : registry.fields_of(world, entity)) {
        names.push_back(ref.component->name() + "." + ref.field->name());
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Position.pos", "Sprite.path", "Sprite.color"}));
}

TEST(FieldRegistry, FieldsOfSkipsComponentsUnknownToWorld) {
    FieldRegistry registry = sample_registry();
    EntityManager world;
    const Entity entity = world.spawn();
    EXPECT_TRUE(registry.fields_of(world, entity).empty());
    EXPECT_EQ(registry.get(world, entity, "Sample", "speed"), std::nullopt);
}

TEST(FieldRegistry, SetOnComponentUnknownToWorldThrows) {
    FieldRegistry registry = sample_registry();
    EntityManager world;
    const Entity entity = world.spawn();
    EXPECT_THROW(registry.set(world, entity, "Sample", "speed", 1.0F),
                 engine::core::ComponentNotRegistered);
}

TEST(CoreFields, RegistersPositionVelocityAndSprite) {
    FieldRegistry registry;
    engine::core::register_core_fields(registry);

    const FieldRef pos = registry.find_field("Position", "pos");
    const FieldRef vel = registry.find_field("Velocity", "vel");
    const FieldRef path = registry.find_field("Sprite", "path");
    const FieldRef color = registry.find_field("Sprite", "color");
    ASSERT_TRUE(pos && vel && path && color);
    EXPECT_EQ(pos.field->kind(), FieldKind::Vec2);
    EXPECT_EQ(vel.field->kind(), FieldKind::Vec2);
    EXPECT_EQ(path.field->kind(), FieldKind::Asset);
    EXPECT_EQ(color.field->kind(), FieldKind::Color);
    EXPECT_EQ(pos.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
    EXPECT_EQ(vel.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
    EXPECT_EQ(path.field->flags(), (FieldFlags{.replicated = false, .editable = true}));
    EXPECT_EQ(color.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
}

TEST(CoreFields, RegisteringTwiceThrows) {
    FieldRegistry registry;
    engine::core::register_core_fields(registry);
    EXPECT_THROW(engine::core::register_core_fields(registry), DuplicateComponentName);
}

TEST_F(FieldRegistryTest, CoreFieldsReadAndWriteComponents) {
    FieldRegistry core;
    engine::core::register_core_fields(core);

    core.set(world, entity, "Position", "pos", Vec2(3.0F, 4.0F));
    core.set(world, entity, "Velocity", "vel", Vec2(-1.0F, 0.5F));
    core.set(world, entity, "Sprite", "path", AssetName{"ship.png"});
    core.set(world, entity, "Sprite", "color", Rgba{.r = 0, .g = 128, .b = 255, .a = 255});

    EXPECT_EQ(world.get<Position>(entity).pos, Vec2(3.0F, 4.0F));
    EXPECT_EQ(world.get<Velocity>(entity).vel, Vec2(-1.0F, 0.5F));
    EXPECT_EQ(world.get<Sprite>(entity).path, "ship.png");
    EXPECT_EQ(core.get(world, entity, "Sprite", "color"),
              FieldValue{(Rgba{.r = 0, .g = 128, .b = 255, .a = 255})});
}
