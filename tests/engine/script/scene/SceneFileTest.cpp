#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "TempFolder.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/Scene.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::AssetName;
using engine::core::EntityManager;
using engine::core::FieldKind;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::script::FieldOverride;
using engine::script::Instance;
using engine::script::InstanceIssue;
using engine::script::Scene;
using engine::script::SceneLoad;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;
using engine::script::write_scene;

namespace {

std::filesystem::path scene_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scenes" / "files" / (name + ".json");
}

std::filesystem::path declarations_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "instances";
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

std::vector<std::size_t> issue_instances(const std::vector<InstanceIssue>& issues) {
    std::vector<std::size_t> indices;
    indices.reserve(issues.size());
    for (const InstanceIssue& issue : issues) {
        indices.push_back(issue.instance);
    }
    return indices;
}

std::vector<std::string> override_fields(const Instance& instance) {
    std::vector<std::string> fields;
    fields.reserve(instance.overrides.size());
    for (const FieldOverride& entry : instance.overrides) {
        fields.push_back(entry.field);
    }
    return fields;
}

FieldList labels(const std::vector<std::string>& names) {
    FieldList list{.element = FieldKind::String, .items = {}};
    for (const std::string& name : names) {
        list.items.emplace_back(name);
    }
    return list;
}

Instance tuned_ship() {
    return {
        .name = "boss_1",
        .type = "ship",
        .position = {840.0F, 300.0F},
        .overrides = {
            {.field = "PhysicsBody.collision_layer", .value = FieldValue{std::int64_t{4}}},
            {.field = "PhysicsBody.mass", .value = FieldValue{0.1F}},
            {.field = "PhysicsBody.resolve_collisions", .value = FieldValue{false}},
            {.field = "Sprite.color", .value = FieldValue{Rgba{.r = 1, .g = 2, .b = 3, .a = 4}}},
            {.field = "Sprite.path", .value = FieldValue{AssetName{"ship.png"}}},
            {.field = "Tags.labels", .value = FieldValue{labels({"boss", "enemy"})}}}};
}

class SceneFileTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    TempFolder _folder;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(declarations_fixture()));
    }

    [[nodiscard]] bool logged(logger::Level level, const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == level && record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] SceneLoad rejected(const std::string& name) const {
        SceneLoad load = _runtime.read_scene(scene_fixture(name));
        EXPECT_FALSE(load.ok());
        EXPECT_EQ(load.scene, Scene{});
        EXPECT_TRUE(load.issues.empty());
        EXPECT_TRUE(logged(logger::Level::Error, load.error));
        return load;
    }
};

} // namespace

TEST_F(SceneFileTest, ReadsInstancesWithSortedOverrides) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("valid"));

    ASSERT_TRUE(load.ok()) << load.error;
    EXPECT_TRUE(load.issues.empty());
    EXPECT_TRUE(load.scene.unknown_instances.empty());
    ASSERT_EQ(load.scene.instances.size(), 2U);
    const Instance& boss = load.scene.instances[0];
    EXPECT_EQ(boss.name, "boss_1");
    EXPECT_EQ(boss.type, "ship");
    EXPECT_EQ(boss.position, (Vec2{840.0F, 300.0F}));
    EXPECT_EQ(override_fields(boss),
              (std::vector<std::string>{"PhysicsBody.mass", "Sprite.color", "Tags.labels"}));
    EXPECT_EQ(boss.overrides[0].value, FieldValue{2.5F});
    EXPECT_EQ(boss.overrides[1].value, (FieldValue{Rgba{.r = 255, .g = 0, .b = 0, .a = 255}}));
    EXPECT_EQ(boss.overrides[2].value, FieldValue{labels({"boss", "enemy"})});
    const Instance& plain = load.scene.instances[1];
    EXPECT_EQ(plain.name, "");
    EXPECT_EQ(plain.type, "plain");
    EXPECT_EQ(plain.position, (Vec2{-12.5F, 0.25F}));
    EXPECT_TRUE(plain.overrides.empty());
}

TEST_F(SceneFileTest, ReadInstancesSpawnWithoutIssues) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("valid"));

    EXPECT_TRUE(_runtime.spawn_instances(load.scene.instances).empty());
    EXPECT_EQ(_runtime.capture_instances(), load.scene.instances);
}

TEST_F(SceneFileTest, SaveThenLoadGivesTheSameScene) {
    const Scene scene{
        .instances = {tuned_ship(),
                      {.name = "", .type = "plain", .position = {0.1F, -1e-3F}, .overrides = {}}},
        .unknown_instances = {}};
    const std::filesystem::path file = _folder.path() / "round_trip.json";

    ASSERT_TRUE(write_scene(file, scene));
    const SceneLoad load = _runtime.read_scene(file);

    ASSERT_TRUE(load.ok()) << load.error;
    EXPECT_TRUE(load.issues.empty());
    EXPECT_EQ(load.scene, scene);
}

TEST_F(SceneFileTest, CapturedWorldRoundTripsThroughAFile) {
    ASSERT_TRUE(_runtime.spawn_instances(std::vector{tuned_ship()}).empty());
    const Scene captured{.instances = _runtime.capture_instances(), .unknown_instances = {}};
    const std::filesystem::path file = _folder.path() / "captured.json";

    ASSERT_TRUE(write_scene(file, captured));

    EXPECT_EQ(_runtime.read_scene(file).scene, captured);
}

TEST_F(SceneFileTest, WritesOverridesSortedAndOmitsEmptyParts) {
    Instance ship = tuned_ship();
    std::ranges::reverse(ship.overrides);
    const Scene scene{
        .instances = {ship, {.name = "", .type = "plain", .position = {}, .overrides = {}}},
        .unknown_instances = {}};

    ASSERT_TRUE(write_scene(_folder.path() / "sorted.json", scene));
    const std::string text = _folder.read("sorted.json");

    EXPECT_LT(text.find("PhysicsBody.mass"), text.find("Tags.labels"));
    EXPECT_EQ(text.find("\"name\": \"\""), std::string::npos);
    EXPECT_EQ(text.find("\"overrides\": {}"), std::string::npos);
    EXPECT_NE(text.find("\"version\": 1"), std::string::npos);
}

TEST_F(SceneFileTest, WritesFloatsWithTheFewestDigits) {
    const Scene scene{.instances = {tuned_ship()}, .unknown_instances = {}};

    ASSERT_TRUE(write_scene(_folder.path() / "floats.json", scene));
    const std::string text = _folder.read("floats.json");

    EXPECT_NE(text.find("0.1"), std::string::npos);
    EXPECT_EQ(text.find("0.100000"), std::string::npos);
}

TEST_F(SceneFileTest, MissingFileIsRejected) {
    const SceneLoad load = rejected("missing");

    EXPECT_NE(load.error.find("cannot open"), std::string::npos);
}

TEST_F(SceneFileTest, TruncatedJsonIsRejected) {
    const SceneLoad load = rejected("truncated");

    EXPECT_NE(load.error.find("not valid JSON"), std::string::npos);
}

TEST_F(SceneFileTest, WrongVersionIsRejected) {
    const SceneLoad load = rejected("wrong_version");

    EXPECT_NE(load.error.find("unsupported version 2"), std::string::npos);
}

TEST_F(SceneFileTest, MissingVersionIsRejected) {
    const SceneLoad load = rejected("no_version");

    EXPECT_NE(load.error.find("no version"), std::string::npos);
}

TEST_F(SceneFileTest, FileThatIsNotAnObjectIsRejected) {
    const SceneLoad load = rejected("not_object");

    EXPECT_NE(load.error.find("JSON object"), std::string::npos);
}

TEST_F(SceneFileTest, InstancesThatAreNotAnArrayAreRejected) {
    const SceneLoad load = rejected("no_instances");

    EXPECT_NE(load.error.find("'instances' array"), std::string::npos);
}

TEST_F(SceneFileTest, UnknownTypesAreReportedAndKept) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("unknown_type"));

    ASSERT_TRUE(load.ok()) << load.error;
    EXPECT_EQ(issue_instances(load.issues), (std::vector<std::size_t>{0, 1}));
    EXPECT_NE(load.issues[0].message.find("dragon"), std::string::npos);
    EXPECT_NE(load.issues[1].message.find("wave"), std::string::npos);
    EXPECT_EQ(load.scene.unknown_instances.size(), 2U);
    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(load.scene.instances[0].type, "plain");
    EXPECT_TRUE(logged(logger::Level::Warn, "dragon"));
}

TEST_F(SceneFileTest, UnknownInstancesAreWrittenBackUnchanged) {
    const SceneLoad first = _runtime.read_scene(scene_fixture("unknown_type"));
    const std::filesystem::path file = _folder.path() / "kept.json";

    ASSERT_TRUE(write_scene(file, first.scene));
    const SceneLoad second = _runtime.read_scene(file);

    EXPECT_EQ(second.scene, first.scene);
    EXPECT_NE(_folder.read("kept.json").find("\"kept\""), std::string::npos);
}

TEST_F(SceneFileTest, UnknownFieldsAreReportedAndSkipped) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("unknown_field"));

    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(override_fields(load.scene.instances[0]),
              (std::vector<std::string>{"PhysicsBody.mass"}));
    EXPECT_EQ(issue_fields(load.issues),
              (std::vector<std::string>{"Ghost.alpha", "Nothing", "Sprite.glow"}));
    EXPECT_TRUE(logged(logger::Level::Warn, "Sprite.glow"));
}

TEST_F(SceneFileTest, WrongKindsAreReportedAndSkipped) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("wrong_kind"));

    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(override_fields(load.scene.instances[0]),
              (std::vector<std::string>{"PhysicsBody.affected_by_gravity"}));
    EXPECT_EQ(issue_fields(load.issues),
              (std::vector<std::string>{"AABBCollider.size", "PhysicsBody.collision_layer",
                                        "PhysicsBody.mass", "Sprite.color", "Tags.labels"}));
}

TEST_F(SceneFileTest, ValuesTheirKindCannotHoldAreReportedOnRead) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("out_of_range"));

    EXPECT_EQ(issue_fields(load.issues),
              (std::vector<std::string>{"PhysicsBody.collision_layer", "PhysicsBody.mass",
                                        "Sprite.color"}));
    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(override_fields(load.scene.instances[0]),
              (std::vector<std::string>{"AABBCollider.size"}));
}

TEST_F(SceneFileTest, ValuesTheComponentRefusesAreReportedOnSpawn) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("out_of_range"));

    const std::vector<InstanceIssue> issues = _runtime.spawn_instances(load.scene.instances);

    EXPECT_EQ(issue_fields(issues), (std::vector<std::string>{"AABBCollider.size"}));
}

TEST_F(SceneFileTest, MalformedInstancesAreReportedAndSkipped) {
    const SceneLoad load = _runtime.read_scene(scene_fixture("bad_instances"));

    ASSERT_TRUE(load.ok()) << load.error;
    EXPECT_EQ(issue_instances(load.issues), (std::vector<std::size_t>{0, 1, 2, 3, 4, 5}));
    ASSERT_EQ(load.scene.instances.size(), 1U);
    EXPECT_EQ(load.scene.instances[0].position, (Vec2{9.0F, 9.0F}));
    EXPECT_TRUE(load.scene.unknown_instances.empty());
}

TEST_F(SceneFileTest, ReadingBeforeScriptsAreLoadedKeepsEveryInstanceAsUnknown) {
    const ScriptRuntime empty = ScriptRuntime::builder().build();

    const SceneLoad load = empty.read_scene(scene_fixture("valid"));

    EXPECT_TRUE(load.scene.instances.empty());
    EXPECT_EQ(load.scene.unknown_instances.size(), 2U);
}

TEST_F(SceneFileTest, WriteCreatesMissingFolders) {
    const std::filesystem::path file = _folder.path() / "scenes" / "deep" / "stage.json";

    EXPECT_TRUE(write_scene(file, Scene{}));
    EXPECT_TRUE(_runtime.read_scene(file).ok());
}

TEST_F(SceneFileTest, InvalidUnknownInstanceFailsWithoutTouchingTheFile) {
    const std::filesystem::path file = _folder.path() / "stage.json";
    const Scene original{.instances = {tuned_ship()}, .unknown_instances = {}};
    ASSERT_TRUE(write_scene(file, original));
    const std::string before = _folder.read("stage.json");

    EXPECT_FALSE(write_scene(file, Scene{.instances = {}, .unknown_instances = {"{ broken"}}));
    EXPECT_FALSE(write_scene(file, Scene{.instances = {}, .unknown_instances = {"[1, 2]"}}));

    EXPECT_EQ(_folder.read("stage.json"), before);
    EXPECT_TRUE(logged(logger::Level::Error, "cannot write"));
}

TEST_F(SceneFileTest, WriteOverAFolderFailsAndLeavesNoTemporaryFile) {
    const std::filesystem::path file = _folder.path() / "taken";
    std::filesystem::create_directories(file / "child");

    EXPECT_FALSE(write_scene(file, Scene{}));

    EXPECT_TRUE(std::filesystem::is_directory(file));
    EXPECT_FALSE(std::filesystem::exists(_folder.path() / "taken.tmp"));
}
