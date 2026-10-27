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
using engine::core::FieldList;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::member_field;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::ScalarValue;
using engine::core::Sprite;
using engine::core::Tags;
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

struct Route {
    std::vector<Vec2> waypoints;
};

struct Body {
    float mass = 1.0F;
};

struct Hull {
    int armor = 5;
};

FieldList waypoints_of(const Route& route) {
    FieldList list{.element = FieldKind::Vec2, .items = {}};
    for (const Vec2 point : route.waypoints) {
        list.items.emplace_back(point);
    }
    return list;
}

void set_waypoints(Route& route, const FieldList& list) {
    if (list.items.size() > 3) {
        throw std::out_of_range("at most 3 waypoints");
    }
    route.waypoints.clear();
    for (const ScalarValue& point : list.items) {
        route.waypoints.push_back(std::get<Vec2>(point));
    }
}

FieldList vec2_list(std::vector<ScalarValue> items) {
    return FieldList{.element = FieldKind::Vec2, .items = std::move(items)};
}

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
    EXPECT_EQ(engine::core::field_kind_name(FieldKind::List), "list");
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
    EXPECT_EQ(engine::core::kind_of(FieldValue{FieldList{}}), FieldKind::List);
    EXPECT_EQ(engine::core::kind_of(ScalarValue{Vec2{}}), FieldKind::Vec2);
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
        EXPECT_EQ(error.reason(), "integer does not fit the member");
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

TEST(CoreFields, RegistersPositionVelocitySpriteAndTags) {
    FieldRegistry registry;
    engine::core::register_core_fields(registry);

    const FieldRef pos = registry.find_field("Position", "pos");
    const FieldRef vel = registry.find_field("Velocity", "vel");
    const FieldRef path = registry.find_field("Sprite", "path");
    const FieldRef color = registry.find_field("Sprite", "color");
    const FieldRef labels = registry.find_field("Tags", "labels");
    ASSERT_TRUE(pos && vel && path && color && labels);
    EXPECT_EQ(labels.field->kind(), FieldKind::List);
    EXPECT_EQ(labels.field->element_kind(), FieldKind::String);
    EXPECT_EQ(labels.field->flags(), (FieldFlags{.replicated = false, .editable = true}));
    EXPECT_EQ(pos.field->element_kind(), std::nullopt);
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

TEST(CoreFields, TagsLabelsReadAndWriteDistinctLabels) {
    FieldRegistry registry;
    engine::core::register_core_fields(registry);
    EntityManager world;
    world.register_component<Tags>();
    const Entity entity = world.spawn();

    registry.set(
        world, entity, "Tags", "labels",
        FieldList{.element = FieldKind::String,
                  .items = {std::string{"enemy"}, std::string{"boss"}, std::string{"enemy"}}});

    EXPECT_TRUE(world.get<Tags>(entity).has("boss"));
    EXPECT_EQ(registry.get(world, entity, "Tags", "labels"),
              FieldValue{(FieldList{.element = FieldKind::String,
                                    .items = {std::string{"enemy"}, std::string{"boss"}}})});
}

class ListFieldTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry.add<Route>("Route",
                            {engine::core::list_field<Route>("waypoints", FieldKind::Vec2, {},
                                                             &waypoints_of, &set_waypoints)});
        world.register_component<Route>();
    }

    FieldRegistry registry;
    EntityManager world;
    Entity entity = world.spawn();
};

TEST_F(ListFieldTest, ReportsListKindAndItemKind) {
    const FieldRef waypoints = registry.find_field("Route", "waypoints");
    ASSERT_TRUE(waypoints);
    EXPECT_EQ(waypoints.field->kind(), FieldKind::List);
    EXPECT_EQ(waypoints.field->element_kind(), FieldKind::Vec2);
}

TEST_F(ListFieldTest, SetsAndGetsItemsInOrder) {
    const FieldList path = vec2_list({Vec2(1.0F, 2.0F), Vec2(3.0F, 4.0F)});
    registry.set(world, entity, "Route", "waypoints", path);
    EXPECT_EQ(world.get<Route>(entity).waypoints,
              (std::vector<Vec2>{Vec2(1.0F, 2.0F), Vec2(3.0F, 4.0F)}));
    EXPECT_EQ(registry.get(world, entity, "Route", "waypoints"), FieldValue{path});
}

TEST_F(ListFieldTest, AcceptsEmptyList) {
    registry.set(world, entity, "Route", "waypoints", vec2_list({}));
    EXPECT_TRUE(world.get<Route>(entity).waypoints.empty());
}

TEST_F(ListFieldTest, RejectsListOfAnotherItemKind) {
    const FieldList names{.element = FieldKind::String, .items = {std::string{"a"}}};
    EXPECT_THROW(registry.set(world, entity, "Route", "waypoints", names), FieldKindMismatch);
    EXPECT_FALSE(world.has<Route>(entity));
}

TEST_F(ListFieldTest, RejectsItemNotMatchingDeclaredItemKind) {
    try {
        registry.set(world, entity, "Route", "waypoints", vec2_list({Vec2{}, 1.0F}));
        FAIL() << "expected FieldKindMismatch";
    } catch (const FieldKindMismatch& error) {
        EXPECT_STREQ(error.what(), "field 'Route.waypoints' expects a list of vec2, not a list of "
                                   "vec2 holding a float");
    }
    EXPECT_FALSE(world.has<Route>(entity));
}

TEST_F(ListFieldTest, WrongItemKindMessageNamesBothLists) {
    try {
        registry.set(world, entity, "Route", "waypoints",
                     FieldList{.element = FieldKind::Int, .items = {}});
        FAIL() << "expected FieldKindMismatch";
    } catch (const FieldKindMismatch& error) {
        EXPECT_STREQ(error.what(),
                     "field 'Route.waypoints' expects a list of vec2, not a list of int");
    }
}

TEST_F(ListFieldTest, RejectsScalarForListField) {
    EXPECT_THROW(registry.set(world, entity, "Route", "waypoints", Vec2{}), FieldKindMismatch);
}

TEST_F(ListFieldTest, WriterCanRejectList) {
    EXPECT_THROW(registry.set(world, entity, "Route", "waypoints",
                              vec2_list({Vec2{}, Vec2{}, Vec2{}, Vec2{}})),
                 FieldValueOutOfRange);
    EXPECT_FALSE(world.has<Route>(entity));
}

TEST(FieldRegistry, RejectsListFieldWithoutItemKind) {
    FieldRegistry registry;
    const FieldDescriptor list = engine::core::component_field<Route>(
        "waypoints", FieldKind::List, {}, [](const Route&) { return FieldValue{FieldList{}}; },
        [](Route&, const FieldValue&) {});
    EXPECT_THROW(registry.add<Route>("Route", {list}), std::invalid_argument);
    EXPECT_TRUE(registry.components().empty());
}

TEST(FieldRegistry, RejectsListOfLists) {
    FieldRegistry registry;
    EXPECT_THROW(registry.add<Route>(
                     "Route", {engine::core::list_field<Route>("waypoints", FieldKind::List, {},
                                                               &waypoints_of, &set_waypoints)}),
                 std::invalid_argument);
    EXPECT_TRUE(registry.components().empty());
}

TEST(FieldRegistry, RejectsItemKindOnScalarField) {
    FieldRegistry registry;
    const FieldDescriptor scalar = member_field<&Body::mass>("mass");
    const FieldDescriptor tagged{"mass",
                                 FieldKind::Float,
                                 {},
                                 [](EntityManager&, Entity) { return FieldValue{1.0F}; },
                                 [](EntityManager&, Entity, const FieldValue&) {},
                                 FieldKind::Float};
    EXPECT_THROW(registry.add<Body>("Body", {tagged}), std::invalid_argument);
    EXPECT_NO_THROW(registry.add<Body>("Body", {scalar}));
}

class RequiredComponentsTest : public ::testing::Test {
protected:
    void SetUp() override {
        registry.add<Hull>("Hull", {member_field<&Hull::armor>("armor")},
                           engine::core::required<Body, Position>);
        world.register_component<Hull>();
        world.register_component<Body>();
        world.register_component<Position>();
    }

    FieldRegistry registry;
    EntityManager world;
    Entity entity = world.spawn();
};

TEST_F(RequiredComponentsTest, SettingFieldAddsMissingRequiredComponents) {
    registry.set(world, entity, "Hull", "armor", std::int64_t{9});
    ASSERT_TRUE(world.has<Body>(entity));
    ASSERT_TRUE(world.has<Position>(entity));
    EXPECT_FLOAT_EQ(world.get<Body>(entity).mass, 1.0F);
}

TEST_F(RequiredComponentsTest, KeepsRequiredComponentsAlreadyPresent) {
    world.add(entity, Body{.mass = 4.0F});
    registry.set(world, entity, "Hull", "armor", std::int64_t{9});
    EXPECT_FLOAT_EQ(world.get<Body>(entity).mass, 4.0F);
}

TEST_F(RequiredComponentsTest, RejectedValueAddsNothing) {
    EXPECT_THROW(registry.set(world, entity, "Hull", "armor", 1.0F), FieldKindMismatch);
    EXPECT_THROW(registry.set(world, entity, "Hull", "armor",
                              std::int64_t{std::numeric_limits<std::int64_t>::max()}),
                 FieldValueOutOfRange);
    EXPECT_FALSE(world.has<Hull>(entity));
    EXPECT_FALSE(world.has<Body>(entity));
    EXPECT_FALSE(world.has<Position>(entity));
}

TEST(RequiredComponents, UnregisteredRequirementThrowsAndChangesNothing) {
    FieldRegistry registry;
    registry.add<Hull>("Hull", {member_field<&Hull::armor>("armor")}, engine::core::required<Body>);
    EntityManager world;
    world.register_component<Hull>();
    const Entity entity = world.spawn();
    EXPECT_THROW(registry.set(world, entity, "Hull", "armor", std::int64_t{1}),
                 engine::core::ComponentNotRegistered);
    EXPECT_FALSE(world.has<Hull>(entity));
}
