#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "TempFolder.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/GameFolder.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::EntityManager;
using engine::core::Vec2;
using engine::script::Instance;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path game_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scenes" / "game";
}

std::vector<std::string> types_of(const std::vector<Instance>& instances) {
    std::vector<std::string> types;
    types.reserve(instances.size());
    for (const Instance& instance : instances) {
        types.push_back(instance.type);
    }
    return types;
}

class GameFolderTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    TempFolder _game;

    void SetUp() override {
        _runtime.attach_world(_world);
        std::filesystem::copy(game_fixture() / "scripts", _game.path() / "scripts",
                              std::filesystem::copy_options::recursive);
        std::filesystem::copy(game_fixture() / "scenes", _game.path() / "scenes",
                              std::filesystem::copy_options::recursive);
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] std::vector<std::string> spawned_types() {
        return types_of(_runtime.capture_instances());
    }
};

} // namespace

TEST_F(GameFolderTest, StartSceneIsSpawnedBeforeOnStart) {
    ASSERT_TRUE(_runtime.load_game(game_fixture()));

    const std::vector<Instance> instances = _runtime.capture_instances();

    ASSERT_EQ(types_of(instances), (std::vector<std::string>{"early", "late"}));
    EXPECT_EQ(instances[0].name, "first");
    EXPECT_EQ(instances[0].position, (Vec2{4.0F, 2.0F}));
}

TEST_F(GameFolderTest, GameWithoutProjectFileLoadsOnlyItsScripts) {
    EXPECT_TRUE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
}

TEST_F(GameFolderTest, ProjectWithoutStartSceneLoadsOnlyItsScripts) {
    _game.write("game.json", R"({ "name": "no-scene" })");

    EXPECT_TRUE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
}

TEST_F(GameFolderTest, InvalidProjectFileIsReportedAndOnStartStillRuns) {
    _game.write("game.json", R"({ "start_scene": )");

    EXPECT_FALSE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
    EXPECT_TRUE(logged_error("game.json is not valid JSON"));
}

TEST_F(GameFolderTest, ProjectThatIsNotAnObjectIsReported) {
    _game.write("game.json", R"(["stage1"])");

    EXPECT_FALSE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
    EXPECT_TRUE(logged_error("must hold a JSON object"));
}

TEST_F(GameFolderTest, StartSceneThatIsNotASceneNameIsRejected) {
    for (const char* name : {R"("../scenes/stage1")", R"("")", "7", R"("stage 1")"}) {
        _game.write("game.json", std::string(R"({ "start_scene": )") + name + " }");
        ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();
        EntityManager world;
        runtime.attach_world(world);

        EXPECT_FALSE(runtime.load_game(_game.path())) << name;

        EXPECT_EQ(types_of(runtime.capture_instances()), (std::vector<std::string>{"late"}))
            << name;
    }
    EXPECT_TRUE(logged_error("start_scene must be a scene name"));
}

TEST_F(GameFolderTest, MissingStartSceneIsReportedAndOnStartStillRuns) {
    _game.write("game.json", R"({ "start_scene": "nowhere" })");

    EXPECT_FALSE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
    EXPECT_TRUE(logged_error("nowhere.json"));
}

TEST_F(GameFolderTest, StartSceneWithInvalidJsonSpawnsNothingOfIt) {
    _game.write("game.json", R"({ "start_scene": "broken" })");
    _game.write("scenes/broken.json", R"({ "version": 1, "instances": [ )");

    EXPECT_FALSE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"late"}));
}

TEST_F(GameFolderTest, SkippedInstancesDoNotFailTheLoad) {
    _game.write("game.json", R"({ "start_scene": "partial" })");
    _game.write("scenes/partial.json", R"({ "version": 1, "instances": [
        { "type": "dragon", "position": [0, 0] },
        { "type": "early", "position": [1, 1], "overrides": { "Nope.field": 1 } }
    ] })");

    EXPECT_TRUE(_runtime.load_game(_game.path()));

    EXPECT_EQ(spawned_types(), (std::vector<std::string>{"early", "late"}));
}

TEST_F(GameFolderTest, StartSceneIsNotSpawnedWithoutAWorld) {
    ScriptRuntime client = ScriptRuntime::builder().build();

    EXPECT_TRUE(client.load_game(game_fixture()));

    EXPECT_TRUE(client.capture_instances().empty());
    EXPECT_FALSE(logged_error("stage1"));
}

TEST_F(GameFolderTest, MissingScriptsFolderFails) {
    _game.write("game.json", R"({ "start_scene": "stage1" })");
    std::filesystem::remove_all(_game.path() / "scripts");

    EXPECT_FALSE(_runtime.load_game(_game.path()));

    EXPECT_TRUE(_runtime.capture_instances().empty());
}

TEST_F(GameFolderTest, LoadDeclarationsRunsNeitherOnStartNorTheStartScene) {
    _game.write("game.json", R"({ "start_scene": "stage1" })");

    EXPECT_TRUE(_runtime.load_declarations(engine::script::scripts_folder(_game.path())));

    std::vector<std::string> types = _runtime.entity_types();
    std::ranges::sort(types);
    EXPECT_EQ(types, (std::vector<std::string>{"early", "late"}));
    EXPECT_TRUE(_runtime.capture_instances().empty());
}

TEST_F(GameFolderTest, WriteStartSceneKeepsTheOtherKeys) {
    _game.write("game.json", R"({ "name": "kept", "start_scene": "stage1", "extra": [1, 2] })");

    ASSERT_TRUE(engine::script::write_start_scene(_game.path(), "stage2"));

    const std::string text = _game.read("game.json");
    EXPECT_NE(text.find("\"kept\""), std::string::npos);
    EXPECT_NE(text.find("\"extra\""), std::string::npos);
    EXPECT_EQ(engine::script::read_project(_game.path())->start_scene, "stage2");
}

TEST_F(GameFolderTest, WriteStartSceneCreatesTheProjectAndRemovesTheKey) {
    ASSERT_TRUE(engine::script::write_start_scene(_game.path(), "stage1"));
    EXPECT_EQ(engine::script::read_project(_game.path())->start_scene, "stage1");

    ASSERT_TRUE(engine::script::write_start_scene(_game.path(), std::nullopt));
    EXPECT_FALSE(engine::script::read_project(_game.path())->start_scene.has_value());
}

TEST_F(GameFolderTest, WriteStartSceneRejectsBadNamesAndBadProjects) {
    EXPECT_FALSE(engine::script::write_start_scene(_game.path(), "../escape"));
    EXPECT_FALSE(std::filesystem::exists(_game.path() / "game.json"));

    _game.write("game.json", R"(["not", "an", "object"])");
    EXPECT_FALSE(engine::script::write_start_scene(_game.path(), "stage1"));
    EXPECT_EQ(_game.read("game.json"), R"(["not", "an", "object"])");

    _game.write("game.json", R"({ "start_scene": )");
    EXPECT_FALSE(engine::script::write_start_scene(_game.path(), "stage1"));
}

TEST_F(GameFolderTest, ListScenesReturnsValidNamesInOrder) {
    _game.write("scenes/b.json", "{}");
    _game.write("scenes/a.json", "{}");
    _game.write("scenes/notes.txt", "");
    _game.write("scenes/bad name.json", "{}");
    _game.write("scenes/nested/c.json", "{}");

    EXPECT_EQ(engine::script::list_scenes(_game.path()),
              (std::vector<std::string>{"a", "b", "stage1"}));
}

TEST_F(GameFolderTest, ListScenesOfAGameWithoutScenesIsEmpty) {
    std::filesystem::remove_all(_game.path() / "scenes");

    EXPECT_TRUE(engine::script::list_scenes(_game.path()).empty());
}
