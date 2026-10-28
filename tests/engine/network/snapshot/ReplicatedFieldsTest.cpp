#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
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
using engine::network::FieldState;
using engine::network::ReplicatedFields;
using engine::network::StringCategory;
using engine::network::StringTable;
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

    EXPECT_EQ(fields.read(world, entity),
              (std::vector<FieldState>{
                  {.field_id = strings.id_of(StringCategory::field, "Label.text").value_or(0),
                   .value = FieldValue{std::string("x")}}}));
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

    const auto states = fields.read(world, entity);

    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front().field_id,
              strings.id_of(StringCategory::field, "Body.pos").value_or(0));
    ASSERT_EQ(sink->records().size(), 2U);
    EXPECT_NE(sink->records()[0].message.find("'Body.spin'"), std::string::npos);
    EXPECT_NE(sink->records()[1].message.find("'Label.text'"), std::string::npos);
}
