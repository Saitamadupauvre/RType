#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/physics/Components.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldValue;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Sprite;
using engine::core::Vec2;
using engine::physics::PhysicsBody;
using engine::script::FieldOverride;
using engine::script::Instance;
using engine::script::InstanceIssue;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path instances_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "instances";
}

struct Scene {
    EntityManager world;
    ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();

    Scene() {
        runtime.attach_world(world);
        EXPECT_TRUE(runtime.load_scripts(instances_fixture()));
    }

    Entity spawn(const std::string& type, std::optional<Vec2> position = std::nullopt) {
        const auto entity = runtime.spawn(type, position);
        EXPECT_TRUE(entity.has_value());
        return entity.value_or(Entity{0});
    }
};

class InstancesTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    Scene _scene;

    [[nodiscard]] bool logged_warning(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Warn &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

bool has_override(const Instance& instance, const std::string& field) {
    return std::ranges::any_of(instance.overrides,
                               [&](const FieldOverride& entry) { return entry.field == field; });
}

std::vector<std::string> override_names(const Instance& instance) {
    std::vector<std::string> names;
    names.reserve(instance.overrides.size());
    for (const FieldOverride& entry : instance.overrides) {
        names.push_back(entry.field);
    }
    return names;
}

} // namespace

TEST_F(InstancesTest, CapturesTypeNameAndPositionInEntityOrder) {
    const Entity ship = _scene.spawn("ship");
    const Entity plain = _scene.spawn("plain", Vec2{5.0F, 6.0F});
    ASSERT_TRUE(_scene.runtime.set_entity_name(plain, "landmark"));
    ASSERT_LT(ship.id(), plain.id());

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 2U);
    EXPECT_EQ(instances[0].type, "ship");
    EXPECT_EQ(instances[0].name, "");
    EXPECT_EQ(instances[0].position, (Vec2{10.0F, 20.0F}));
    EXPECT_EQ(instances[1].type, "plain");
    EXPECT_EQ(instances[1].name, "landmark");
    EXPECT_EQ(instances[1].position, (Vec2{5.0F, 6.0F}));
}

TEST_F(InstancesTest, FreshEntityHasNoOverrides) {
    _scene.spawn("ship", Vec2{300.0F, 40.0F});
    _scene.spawn("plain");

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 2U);
    EXPECT_TRUE(instances[0].overrides.empty());
    EXPECT_TRUE(instances[1].overrides.empty());
}

TEST_F(InstancesTest, StoresOnlyFieldsThatDifferFromThePrefab) {
    const Entity ship = _scene.spawn("ship");
    _scene.world.get<Sprite>(ship).color = Rgba{.r = 1, .g = 2, .b = 3, .a = 4};

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 1U);
    ASSERT_EQ(instances[0].overrides.size(), 1U);
    EXPECT_EQ(instances[0].overrides[0],
              (FieldOverride{.field = "Sprite.color",
                             .value = FieldValue{Rgba{.r = 1, .g = 2, .b = 3, .a = 4}}}));
}

TEST_F(InstancesTest, StoresEveryFieldOfAComponentThePrefabLacks) {
    const Entity plain = _scene.spawn("plain");
    _scene.runtime.fields().set(_scene.world, plain, "PhysicsBody", "mass", FieldValue{2.5F});

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 1U);
    EXPECT_TRUE(has_override(instances[0], "PhysicsBody.mass"));
    EXPECT_TRUE(has_override(instances[0], "PhysicsBody.type"));
    EXPECT_FALSE(has_override(instances[0], "Position.pos"));
}

TEST_F(InstancesTest, OverridesAreSortedByFieldName) {
    const Entity plain = _scene.spawn("plain");
    _scene.runtime.fields().set(_scene.world, plain, "PhysicsBody", "mass", FieldValue{2.5F});
    _scene.runtime.fields().set(_scene.world, plain, "Sprite", "path",
                                FieldValue{engine::core::AssetName{"rock.png"}});

    const std::vector<std::string> names = override_names(_scene.runtime.capture_instances().at(0));

    EXPECT_TRUE(std::ranges::is_sorted(names));
}

TEST_F(InstancesTest, WhatOnSpawnSetsIsAnOverride) {
    _scene.spawn("painted");

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 1U);
    EXPECT_EQ(override_names(instances[0]), std::vector<std::string>{"Sprite.color"});
}

TEST_F(InstancesTest, SkipsDestroyedEntitiesAndEntitiesNotSpawnedByScripts) {
    _scene.world.add(_scene.world.spawn(), Position{});
    _scene.spawn("ship");
    ASSERT_TRUE(_scene.runtime.run_string("spawn('plain'):destroy()", "test"));

    const std::vector<Instance> instances = _scene.runtime.capture_instances();

    ASSERT_EQ(instances.size(), 1U);
    EXPECT_EQ(instances[0].type, "ship");
}

TEST_F(InstancesTest, CaptureWithoutWorldIsEmpty) {
    ScriptRuntime detached = ScriptRuntime::builder().build();
    EXPECT_TRUE(detached.capture_instances().empty());
}

TEST_F(InstancesTest, RoundTripRebuildsTheSameInstances) {
    const Entity ship = _scene.spawn("ship", Vec2{50.0F, 60.0F});
    _scene.world.get<Sprite>(ship).color = Rgba{.r = 9, .g = 8, .b = 7, .a = 6};
    _scene.runtime.set_entity_name(ship, "hero");
    const Entity painted = _scene.spawn("painted", Vec2{1.0F, 2.0F});
    _scene.world.get<Sprite>(painted).color = Rgba{.r = 0, .g = 0, .b = 255, .a = 255};
    const Entity plain = _scene.spawn("plain");
    _scene.runtime.fields().set(_scene.world, plain, "PhysicsBody", "mass", FieldValue{4.0F});
    const std::vector<Instance> captured = _scene.runtime.capture_instances();

    Scene rebuilt;
    const std::vector<InstanceIssue> issues = rebuilt.runtime.spawn_instances(captured);

    EXPECT_TRUE(issues.empty());
    EXPECT_EQ(rebuilt.runtime.capture_instances(), captured);
}

TEST_F(InstancesTest, OverridesWinOverOnSpawn) {
    const std::vector<Instance> instances{
        {.name = "",
         .type = "painted",
         .position = {},
         .overrides = {{.field = "Sprite.color",
                        .value = FieldValue{Rgba{.r = 0, .g = 255, .b = 0, .a = 255}}}}}};

    ASSERT_TRUE(_scene.runtime.spawn_instances(instances).empty());

    std::optional<Rgba> color;
    _scene.world.query<Sprite>().each([&](Entity, Sprite& sprite) { color = sprite.color; });
    EXPECT_EQ(color, (Rgba{.r = 0, .g = 255, .b = 0, .a = 255}));
}

TEST_F(InstancesTest, SpawnedInstanceKeepsItsName) {
    const std::vector<Instance> instances{
        {.name = "gate", .type = "plain", .position = {3.0F, 4.0F}, .overrides = {}}};

    ASSERT_TRUE(_scene.runtime.spawn_instances(instances).empty());

    const std::vector<Instance> captured = _scene.runtime.capture_instances();
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured[0].name, "gate");
    EXPECT_EQ(captured[0].position, (Vec2{3.0F, 4.0F}));
}

TEST_F(InstancesTest, KeepsEntitiesAlreadyInTheWorld) {
    _scene.spawn("ship");
    const std::vector<Instance> instances{
        {.name = "", .type = "plain", .position = {}, .overrides = {}}};

    ASSERT_TRUE(_scene.runtime.spawn_instances(instances).empty());

    EXPECT_EQ(_scene.runtime.capture_instances().size(), 2U);
}

TEST_F(InstancesTest, UnknownTypeIsReportedAndSkipped) {
    const std::vector<Instance> instances{
        {.name = "", .type = "ghost", .position = {}, .overrides = {}},
        {.name = "", .type = "plain", .position = {}, .overrides = {}}};

    const std::vector<InstanceIssue> issues = _scene.runtime.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].instance, 0U);
    EXPECT_TRUE(issues[0].field.empty());
    EXPECT_NE(issues[0].message.find("ghost"), std::string::npos);
    EXPECT_TRUE(logged_warning("ghost"));
    const std::vector<Instance> captured = _scene.runtime.capture_instances();
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_EQ(captured[0].type, "plain");
}

TEST_F(InstancesTest, SystemTypeIsReportedAndSkipped) {
    const std::vector<Instance> instances{
        {.name = "", .type = "wave", .position = {}, .overrides = {}}};

    const std::vector<InstanceIssue> issues = _scene.runtime.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 1U);
    EXPECT_NE(issues[0].message.find("wave"), std::string::npos);
    EXPECT_TRUE(_scene.runtime.capture_instances().empty());
}

TEST_F(InstancesTest, UnknownFieldsAreReportedAndTheEntityIsKept) {
    const std::vector<Instance> instances{
        {.name = "",
         .type = "plain",
         .position = {},
         .overrides = {{.field = "Sprite.glow", .value = FieldValue{1.0F}},
                       {.field = "Ghost.size", .value = FieldValue{1.0F}},
                       {.field = "nodot", .value = FieldValue{1.0F}},
                       {.field = "PhysicsBody.mass", .value = FieldValue{3.0F}}}}};

    const std::vector<InstanceIssue> issues = _scene.runtime.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 3U);
    EXPECT_EQ(issues[0].field, "Sprite.glow");
    EXPECT_EQ(issues[1].field, "Ghost.size");
    EXPECT_EQ(issues[2].field, "nodot");
    EXPECT_TRUE(logged_warning("Sprite.glow"));
    const std::vector<Instance> captured = _scene.runtime.capture_instances();
    ASSERT_EQ(captured.size(), 1U);
    EXPECT_TRUE(has_override(captured[0], "PhysicsBody.mass"));
}

TEST_F(InstancesTest, WrongKindIsReported) {
    const std::vector<Instance> instances{
        {.name = "",
         .type = "ship",
         .position = {},
         .overrides = {{.field = "Sprite.color", .value = FieldValue{1.0F}}}}};

    const std::vector<InstanceIssue> issues = _scene.runtime.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].field, "Sprite.color");
    std::optional<Rgba> color;
    _scene.world.query<Sprite>().each([&](Entity, Sprite& sprite) { color = sprite.color; });
    EXPECT_EQ(color, Rgba{});
}

TEST_F(InstancesTest, OutOfRangeValueIsReported) {
    const std::vector<Instance> instances{
        {.name = "",
         .type = "ship",
         .position = {},
         .overrides = {{.field = "AABBCollider.size", .value = FieldValue{Vec2{-1.0F, 2.0F}}}}}};

    const std::vector<InstanceIssue> issues = _scene.runtime.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 1U);
    EXPECT_EQ(issues[0].field, "AABBCollider.size");
    EXPECT_TRUE(_scene.runtime.capture_instances().at(0).overrides.empty());
}

TEST_F(InstancesTest, SpawningWithoutWorldReportsEveryInstance) {
    ScriptRuntime detached = ScriptRuntime::builder().build();
    ASSERT_TRUE(detached.load_scripts(instances_fixture()));
    const std::vector<Instance> instances{
        {.name = "", .type = "plain", .position = {}, .overrides = {}}};

    const std::vector<InstanceIssue> issues = detached.spawn_instances(instances);

    ASSERT_EQ(issues.size(), 1U);
    EXPECT_TRUE(issues[0].field.empty());
}

TEST_F(InstancesTest, NamesOnlyExistForScriptEntities) {
    const Entity plain = _scene.spawn("plain");
    const Entity foreign = _scene.world.spawn();

    EXPECT_EQ(_scene.runtime.entity_name(plain), "");
    EXPECT_FALSE(_scene.runtime.set_entity_name(foreign, "nope"));
    EXPECT_EQ(_scene.runtime.entity_name(foreign), std::nullopt);
    _scene.runtime.release(plain);
    EXPECT_EQ(_scene.runtime.entity_name(plain), std::nullopt);
}
