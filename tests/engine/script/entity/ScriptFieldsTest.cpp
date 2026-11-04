#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../scene/TempFolder.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/Scene.hpp"
#include "engine/script/ScriptField.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldKind;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::ScalarValue;
using engine::core::Sprite;
using engine::core::Vec2;
using engine::script::FieldOverride;
using engine::script::Instance;
using engine::script::InstanceIssue;
using engine::script::Scene;
using engine::script::SceneLoad;
using engine::script::ScriptField;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;
using engine::script::write_scene;

namespace {

std::filesystem::path scripts_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "script_fields";
}

std::filesystem::path scene_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scenes" / "files" / "script_fields.json";
}

struct World {
    EntityManager entities;
    ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();

    World() {
        runtime.attach_world(entities);
        EXPECT_TRUE(runtime.load_scripts(scripts_fixture()));
    }

    Entity spawn(const std::string& type) {
        const auto entity = runtime.spawn(type);
        EXPECT_TRUE(entity.has_value());
        return entity.value_or(Entity{0});
    }

    [[nodiscard]] bool check(const std::string& condition) {
        return runtime.run_string("assert(" + condition + ")", "check");
    }
};

class ScriptFieldsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    World _world;

    [[nodiscard]] bool logged_warning(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Warn &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] std::optional<ScriptField> find_field(const std::string& name) const {
        const std::vector<ScriptField> fields = _world.runtime.script_fields("tuned");
        const auto found = std::ranges::find(fields, name, &ScriptField::name);
        if (found == fields.end()) {
            return std::nullopt;
        }
        return *found;
    }

    [[nodiscard]] ScriptField field(const std::string& name) const {
        std::optional<ScriptField> found = find_field(name);
        if (!found) {
            ADD_FAILURE() << "no script field " << name;
            return {};
        }
        return *found;
    }
};

std::vector<std::string> field_names(const std::vector<ScriptField>& fields) {
    std::vector<std::string> names;
    names.reserve(fields.size());
    for (const ScriptField& field : fields) {
        names.push_back(field.name);
    }
    return names;
}

std::vector<std::string> override_names(const Instance& instance) {
    std::vector<std::string> names;
    names.reserve(instance.overrides.size());
    for (const FieldOverride& entry : instance.overrides) {
        names.push_back(entry.field);
    }
    return names;
}

std::vector<std::string> issue_fields(const std::vector<InstanceIssue>& issues) {
    std::vector<std::string> fields;
    fields.reserve(issues.size());
    for (const InstanceIssue& issue : issues) {
        fields.push_back(issue.field);
    }
    std::ranges::sort(fields);
    return fields;
}

FieldList list_of(FieldKind element, std::vector<ScalarValue> items) {
    return FieldList{.element = element, .items = std::move(items)};
}

Instance tuned_with(std::vector<FieldOverride> overrides) {
    return {.name = "", .type = "tuned", .position = {}, .overrides = std::move(overrides)};
}

} // namespace

TEST_F(ScriptFieldsTest, ListsSupportedFreeFieldsSortedByName) {
    EXPECT_EQ(field_names(_world.runtime.script_fields("tuned")),
              (std::vector<std::string>{"aim", "alive", "glow", "health", "names", "ratio", "speed",
                                        "steps", "tint", "title", "waypoints", "weights"}));
}

TEST_F(ScriptFieldsTest, IntegersAndFloatsFollowTheLuaSubtype) {
    EXPECT_EQ(field("health"),
              (ScriptField{.name = "health", .default_value = FieldValue{std::int64_t{3}}}));
    EXPECT_EQ(field("speed"), (ScriptField{.name = "speed", .default_value = FieldValue{2.5F}}));
    EXPECT_EQ(field("ratio"), (ScriptField{.name = "ratio", .default_value = FieldValue{3.0F}}));
}

TEST_F(ScriptFieldsTest, InfersBooleansAndStrings) {
    EXPECT_EQ(field("alive").kind(), FieldKind::Bool);
    EXPECT_EQ(field("title").default_value, FieldValue{std::string{"boss"}});
}

TEST_F(ScriptFieldsTest, InfersVec2AndColorRecords) {
    EXPECT_EQ(field("aim").default_value, (FieldValue{Vec2{1.0F, 2.0F}}));
    EXPECT_EQ(field("tint").default_value, (FieldValue{Rgba{.r = 255, .g = 0, .b = 0, .a = 255}}));
    EXPECT_EQ(field("glow").default_value, (FieldValue{Rgba{.r = 1, .g = 2, .b = 3, .a = 4}}));
}

TEST_F(ScriptFieldsTest, InfersListsAndWidensMixedNumbersToFloat) {
    EXPECT_EQ(
        field("steps").default_value,
        (FieldValue{list_of(FieldKind::Int, {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}})}));
    EXPECT_EQ(field("weights").default_value,
              (FieldValue{list_of(FieldKind::Float, {1.0F, 2.5F})}));
    EXPECT_EQ(field("names").element(), FieldKind::String);
    EXPECT_EQ(field("waypoints").default_value,
              (FieldValue{list_of(FieldKind::Vec2, {Vec2{0.0F, 0.0F}, Vec2{4.0F, 5.0F}})}));
}

TEST_F(ScriptFieldsTest, SkipsUnsupportedValuesPrivateNamesAndNativeFields) {
    for (const char* name : {"_secret", "empty", "nested", "mixed", "map", "point3", "bad_tint",
                             "huge", "wound", "sprite", "tags", "server", "server_only"}) {
        EXPECT_EQ(find_field(name), std::nullopt) << name;
    }
}

TEST_F(ScriptFieldsTest, UnknownTypesAndSystemsHaveNoScriptFields) {
    EXPECT_TRUE(_world.runtime.script_fields("ghost").empty());
    EXPECT_TRUE(_world.runtime.script_fields("patrol").empty());
}

TEST_F(ScriptFieldsTest, FreshEntityHasNoScriptOverrides) {
    _world.spawn("tuned");

    EXPECT_TRUE(_world.runtime.capture_instances().at(0).overrides.empty());
}

TEST_F(ScriptFieldsTest, CapturesChangedScriptFields) {
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "wound"));

    const std::vector<FieldOverride> overrides = _world.runtime.capture_instances().at(0).overrides;

    EXPECT_EQ(overrides, (std::vector<FieldOverride>{
                             {.field = "Script.aim", .value = FieldValue{Vec2{5.0F, 6.0F}}},
                             {.field = "Script.health", .value = FieldValue{std::int64_t{1}}}}));
}

TEST_F(ScriptFieldsTest, CapturesAnInPlaceChangeOfAListElement) {
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "rename_first"));

    EXPECT_EQ(override_names(_world.runtime.capture_instances().at(0)),
              std::vector<std::string>{"Script.names"});
    EXPECT_TRUE(_world.check("spawn('tuned').names[1] == 'a'"));
}

TEST_F(ScriptFieldsTest, ValueEqualToTheDefaultIsNotAnOverride) {
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "heal_as_float"));

    EXPECT_TRUE(_world.runtime.capture_instances().at(0).overrides.empty());
}

TEST_F(ScriptFieldsTest, ValueOfTheWrongKindIsNotCapturedAndIsLogged) {
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "break_health"));

    EXPECT_TRUE(_world.runtime.capture_instances().at(0).overrides.empty());
    EXPECT_TRUE(logged_warning("health"));
}

TEST_F(ScriptFieldsTest, WhatOnSpawnSetsIsAnOverride) {
    _world.spawn("armored");

    EXPECT_EQ(_world.runtime.capture_instances().at(0).overrides,
              (std::vector<FieldOverride>{
                  {.field = "Script.health", .value = FieldValue{std::int64_t{20}}}}));
}

TEST_F(ScriptFieldsTest, OverridesWinOverOnSpawn) {
    const std::vector<Instance> instances{
        {.name = "",
         .type = "armored",
         .position = {},
         .overrides = {{.field = "Script.health", .value = FieldValue{std::int64_t{7}}}}}};

    ASSERT_TRUE(_world.runtime.spawn_instances(instances).empty());

    EXPECT_EQ(_world.runtime.capture_instances().at(0).overrides, instances[0].overrides);
}

TEST_F(ScriptFieldsTest, SpawnedOverridesAreVisibleToLua) {
    const std::vector<Instance> instances{
        tuned_with({{.field = "Script.aim", .value = FieldValue{Vec2{7.0F, 8.0F}}},
                    {.field = "Script.steps",
                     .value = FieldValue{list_of(FieldKind::Int, {std::int64_t{4}})}}})};

    ASSERT_TRUE(_world.runtime.spawn_instances(instances).empty());

    std::optional<Entity> spawned;
    _world.entities.query<Sprite>().each([&](Entity entity, Sprite&) { spawned = entity; });
    ASSERT_TRUE(spawned.has_value());
    EXPECT_TRUE(_world.runtime.call(spawned.value_or(Entity{0}), "check_spawned"));
}

TEST_F(ScriptFieldsTest, RoundTripRebuildsTheSameInstances) {
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "wound"));
    ASSERT_TRUE(_world.runtime.call(entity, "rename_first"));
    const std::vector<Instance> captured = _world.runtime.capture_instances();

    World rebuilt;
    ASSERT_TRUE(rebuilt.runtime.spawn_instances(captured).empty());

    EXPECT_EQ(rebuilt.runtime.capture_instances(), captured);
}

TEST_F(ScriptFieldsTest, UnknownAndMistypedScriptFieldsAreReported) {
    const std::vector<Instance> instances{tuned_with(
        {{.field = "Script.mana", .value = FieldValue{std::int64_t{1}}},
         {.field = "Script.health", .value = FieldValue{std::string{"lots"}}},
         {.field = "Script.steps", .value = FieldValue{list_of(FieldKind::Float, {1.5F})}},
         {.field = "Script.server_only", .value = FieldValue{std::int64_t{1}}},
         {.field = "Script.speed", .value = FieldValue{4.0F}}})};

    const std::vector<InstanceIssue> issues = _world.runtime.spawn_instances(instances);

    EXPECT_EQ(issue_fields(issues),
              (std::vector<std::string>{"Script.health", "Script.mana", "Script.server_only",
                                        "Script.steps"}));
    EXPECT_TRUE(logged_warning("Script.mana"));
    EXPECT_EQ(_world.runtime.capture_instances().at(0).overrides,
              (std::vector<FieldOverride>{{.field = "Script.speed", .value = FieldValue{4.0F}}}));
}

TEST_F(ScriptFieldsTest, SceneFileReadsScriptOverridesAndReportsBadOnes) {
    const SceneLoad load = _world.runtime.read_scene(scene_fixture());

    ASSERT_TRUE(load.ok());
    EXPECT_EQ(issue_fields(load.issues),
              (std::vector<std::string>{"Script._secret", "Script.mana", "Script.server_only",
                                        "Script.speed", "Script.steps"}));
    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(
        load.scene.instances[0].overrides,
        (std::vector<FieldOverride>{
            {.field = "Script.health", .value = FieldValue{std::int64_t{9}}},
            {.field = "Script.tint", .value = FieldValue{Rgba{.r = 0, .g = 255, .b = 0, .a = 255}}},
            {.field = "Script.waypoints",
             .value = FieldValue{list_of(FieldKind::Vec2, {Vec2{1.0F, 2.0F}})}}}));
}

TEST_F(ScriptFieldsTest, SceneFileRoundTripKeepsScriptOverrides) {
    const TempFolder folder;
    const Entity entity = _world.spawn("tuned");
    ASSERT_TRUE(_world.runtime.call(entity, "wound"));
    const Scene scene{.instances = _world.runtime.capture_instances(), .unknown_instances = {}};

    ASSERT_TRUE(write_scene(folder.path() / "scene.json", scene));
    const SceneLoad load = _world.runtime.read_scene(folder.path() / "scene.json");

    ASSERT_TRUE(load.ok());
    EXPECT_TRUE(load.issues.empty());
    EXPECT_EQ(load.scene, scene);
}
