#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Transform.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/network/StringTable.hpp"

#include "TestFields.hpp"
#include "snapshot/ReplicatedFields.hpp"

namespace logger = engine::core::log;

namespace {

using engine::core::FieldDescriptor;
using engine::core::FieldKind;
using engine::core::FieldList;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::field_flags::replicated;
using engine::network::AssetId;
using engine::network::FieldState;
using engine::network::ReplicatedFields;
using engine::network::StringCategory;
using engine::network::StringTable;
using engine::network::WireValue;
using test_fields::Body;
using test_fields::Label;

struct Wide {
    std::string text;
};

FieldDescriptor wide_text(std::string name) {
    return engine::core::component_field<Wide>(
        std::move(name), FieldKind::String, replicated,
        [](const Wide& wide) { return FieldValue{wide.text}; },
        [](Wide& wide, const FieldValue& value) { wide.text = std::get<std::string>(value); });
}

FieldRegistry wide_registry(std::size_t field_count) {
    std::vector<FieldDescriptor> fields;
    for (std::size_t index = 0; index < field_count; ++index) {
        fields.push_back(wide_text("t" + std::to_string(index)));
    }
    FieldRegistry registry;
    registry.add<Wide>("Wide", std::move(fields));
    return registry;
}

struct Skin {
    std::string path;
};

FieldRegistry skin_registry() {
    FieldRegistry registry;
    registry.add<Skin>("Skin", {engine::core::asset_field<&Skin::path>("path", replicated)});
    return registry;
}

std::vector<std::string> field_names(const StringTable& strings) {
    std::vector<std::string> names;
    for (const auto& entry : strings.entries(StringCategory::field)) {
        names.push_back(entry.name);
    }
    return names;
}

} // namespace

TEST(ReplicatedFields, NamesEveryReplicatedFieldInRegistrationOrder) {
    const FieldRegistry registry = test_fields::registry();
    StringTable strings;

    const ReplicatedFields fields(registry, strings);

    EXPECT_TRUE(fields.error().empty());
    EXPECT_EQ(field_names(strings), (std::vector<std::string>{"Body.pos", "Body.spin", "Label.text",
                                                              "Replicated.owner"}));
    ASSERT_EQ(fields.fields().size(), 4U);
    EXPECT_EQ(fields.fields()[1].id, strings.id_of(StringCategory::field, "Body.spin"));
    EXPECT_EQ(fields.fields()[1].field.field, registry.find_field("Body", "spin").field);
}

TEST(ReplicatedFields, KeepsTheIdOfANameAlreadyInTheTable) {
    const FieldRegistry registry = test_fields::registry();
    StringTable strings;
    ASSERT_TRUE(strings.insert({.category = StringCategory::field, .id = 40, .name = "Body.spin"}));

    const ReplicatedFields fields(registry, strings);

    EXPECT_TRUE(fields.error().empty());
    EXPECT_EQ(fields.fields()[1].id, 40U);
    EXPECT_EQ(strings.entries(StringCategory::field).size(), 4U);
}

TEST(ReplicatedFields, RejectsReplicatedListField) {
    FieldRegistry registry;
    registry.add<Label>(
        "Label", {engine::core::list_field<Label>(
                     "letters", FieldKind::String, replicated,
                     [](const Label&) { return FieldList{}; }, [](Label&, const FieldList&) {})});
    StringTable strings;

    const ReplicatedFields fields(registry, strings);

    EXPECT_EQ(fields.error(), "field 'Label.letters' is a list, which snapshots cannot carry");
    EXPECT_TRUE(fields.fields().empty());
}

TEST(ReplicatedFields, RejectsNameTooLongForTheStringTable) {
    FieldRegistry registry;
    registry.add<Wide>("Wide", {wide_text(std::string(60, 'n'))});
    StringTable strings;

    const ReplicatedFields fields(registry, strings);

    EXPECT_NE(fields.error().find("cannot be added to the string table"), std::string::npos)
        << fields.error();
    EXPECT_TRUE(fields.fields().empty());
}

TEST(ReplicatedFields, RejectsMoreThan255Fields) {
    StringTable strings;
    const FieldRegistry many = wide_registry(256);
    const ReplicatedFields too_many(many, strings);
    EXPECT_EQ(too_many.error(), "more than 255 fields are replicated");
}

TEST(ReplicatedFields, RejectsFieldsThatMayNotFitInOneSnapshot) {
    StringTable fitting_strings;
    const FieldRegistry fitting = wide_registry(4);
    EXPECT_TRUE(ReplicatedFields(fitting, fitting_strings).error().empty());

    StringTable strings;
    const FieldRegistry oversized = wide_registry(5);
    const ReplicatedFields fields(oversized, strings);

    EXPECT_NE(fields.error().find("more than the 1181 a snapshot holds"), std::string::npos)
        << fields.error();
}

TEST(ReplicatedFields, ReadsOnlyTheComponentsTheEntityOwns) {
    const FieldRegistry registry = test_fields::registry();
    StringTable strings;
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    test_fields::register_components(world);
    const auto entity = world.spawn();
    world.add(entity, Label{.text = "x", .hidden = false});

    EXPECT_EQ(fields.read(world, entity, strings),
              (std::vector<FieldState>{
                  {.field_id = strings.id_of(StringCategory::field, "Label.text").value_or(0),
                   .value = std::string("x")}}));
}

TEST(ReplicatedFields, SendsAnAssetAsItsTextureId) {
    const FieldRegistry registry = skin_registry();
    StringTable strings;
    const auto ship = strings.add(StringCategory::texture, "sprites/ship.png");
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    world.register_component<Skin>();
    const auto entity = world.spawn();
    world.add(entity, Skin{.path = "sprites/ship.png"});

    const auto states = fields.read(world, entity, strings);

    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front().value, WireValue{AssetId{ship.value_or(0)}});
}

TEST(ReplicatedFields, SendsAnEmptyAssetAsNoAsset) {
    const FieldRegistry registry = skin_registry();
    StringTable strings;
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    world.register_component<Skin>();
    const auto entity = world.spawn();
    world.add(entity, Skin{});

    const auto states = fields.read(world, entity, strings);

    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front().value, WireValue{AssetId{engine::network::no_asset}});
}

TEST(ReplicatedFields, LeavesOutAndLogsATextureMissingFromTheTable) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped{sink, logger::Level::Trace};
    const FieldRegistry registry = skin_registry();
    StringTable strings;
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    world.register_component<Skin>();
    const auto entity = world.spawn();
    world.add(entity, Skin{.path = "sprites/unknown.png"});

    EXPECT_TRUE(fields.read(world, entity, strings).empty());
    ASSERT_EQ(sink->records().size(), 1U);
    EXPECT_NE(sink->records()[0].message.find("'Skin.path'"), std::string::npos);
}

TEST(ReplicatedFields, LeavesOutAndLogsValuesTheWireCannotCarry) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped{sink, logger::Level::Trace};
    const FieldRegistry registry = test_fields::registry();
    StringTable strings;
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    test_fields::register_components(world);
    const auto entity = world.spawn();
    world.add(entity, Label{.text = std::string(256, 'a'), .hidden = false});
    world.add(entity, Body{.pos = {1.F, 2.F}, .spin = std::numeric_limits<float>::quiet_NaN()});

    const auto states = fields.read(world, entity, strings);

    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front().field_id,
              strings.id_of(StringCategory::field, "Body.pos").value_or(0));
    ASSERT_EQ(sink->records().size(), 2U);
    EXPECT_NE(sink->records()[0].message.find("'Body.spin'"), std::string::npos);
    EXPECT_NE(sink->records()[1].message.find("'Label.text'"), std::string::npos);
}

namespace {

FieldRegistry core_registry() {
    FieldRegistry registry;
    engine::core::register_core_fields(registry);
    return registry;
}

class CoreReplicatedFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        _world.register_component<engine::core::Position>();
        _world.register_component<engine::core::Rotation>();
        _world.register_component<engine::core::Scale>();
        _entity = _world.spawn();
        _world.add(_entity, engine::core::Position{1.F, 2.F});
    }

    [[nodiscard]] std::vector<std::string> sent_names() {
        std::vector<std::string> names;
        for (const FieldState& state : _fields.read(_world, _entity, _strings)) {
            const std::string* name = _strings.name_of(StringCategory::field, state.field_id);
            names.push_back(name == nullptr ? "" : *name);
        }
        return names;
    }

    FieldRegistry _registry = core_registry();
    StringTable _strings;
    ReplicatedFields _fields{_registry, _strings};
    engine::core::EntityManager _world;
    engine::core::Entity _entity{0, 0};
};

} // namespace

TEST_F(CoreReplicatedFieldsTest, EntityWithoutRotationOrScaleSendsNeither) {
    EXPECT_EQ(sent_names(), std::vector<std::string>{"Position.pos"});
}

TEST_F(CoreReplicatedFieldsTest, EntityWithRotationAndScaleSendsBoth) {
    _world.add(_entity, engine::core::Rotation{.degrees = 45.F});
    _world.add(_entity, engine::core::Scale{.factor = {-1.F, 0.25F}});

    const auto states = _fields.read(_world, _entity, _strings);

    EXPECT_EQ(sent_names(),
              (std::vector<std::string>{"Position.pos", "Rotation.degrees", "Scale.x", "Scale.y"}));
    ASSERT_EQ(states.size(), 4U);
    EXPECT_EQ(states[1].value, WireValue{45.F});
    EXPECT_EQ(states[2].value, WireValue{-1.F});
    EXPECT_EQ(states[3].value, WireValue{0.25F});
}

TEST_F(CoreReplicatedFieldsTest, CoreFieldsFitInOneSnapshot) {
    EXPECT_TRUE(_fields.error().empty()) << _fields.error();
}

TEST(ReplicatedFields, SendsAShaderAssetAsItsShaderId) {
    FieldRegistry registry;
    registry.add<Skin>("Skin", {engine::core::asset_field<&Skin::path>(
                                   "path", replicated, engine::core::AssetType::Shader)});
    StringTable strings;
    strings.add(StringCategory::texture, "shaders/space");
    strings.add(StringCategory::shader, "shaders/other");
    const auto space = strings.add(StringCategory::shader, "shaders/space");
    ASSERT_NE(space, strings.id_of(StringCategory::texture, "shaders/space"));
    const ReplicatedFields fields(registry, strings);
    engine::core::EntityManager world;
    world.register_component<Skin>();
    const auto entity = world.spawn();
    world.add(entity, Skin{.path = "shaders/space"});

    const auto states = fields.read(world, entity, strings);

    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front().value, WireValue{AssetId{space.value_or(0)}});
}
