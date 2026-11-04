#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/ScriptField.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::script::ClientApi;
using engine::script::EventCommand;
using engine::script::ScriptField;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path switch_game() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scenes" / "switch";
}

class SceneSwitchTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_game(switch_game()));
    }

    [[nodiscard]] std::vector<std::string> types() {
        std::vector<std::string> names;
        _world.query<Position>().each([&](Entity entity, const Position&) {
            names.push_back(_runtime.entity_type(entity).value_or("?"));
        });
        std::ranges::sort(names);
        return names;
    }

    [[nodiscard]] std::optional<Entity> entity_of(const std::string& type) {
        std::optional<Entity> found;
        _world.query<Position>().each([&](Entity entity, const Position&) {
            if (_runtime.entity_type(entity) == type) {
                found = entity;
            }
        });
        return found;
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    void expect_rejected(const std::string& code, const std::string& error) {
        EXPECT_FALSE(_runtime.run_string(code, "switch")) << code;
        EXPECT_TRUE(logged_error(error)) << code;
        EXPECT_EQ(_runtime.flush_scene(), std::nullopt) << code;
        EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "rock"})) << code;
    }
};

} // namespace

TEST_F(SceneSwitchTest, StartSceneIsSpawned) {
    EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "rock"}));
}

TEST_F(SceneSwitchTest, LoadSceneWaitsForFlushScene) {
    ASSERT_TRUE(_runtime.run_string("load_scene('next')", "switch"));

    EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "rock"}));
    EXPECT_EQ(_runtime.flush_scene(), "next");
    EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "star"}));
}

TEST_F(SceneSwitchTest, SpawnsTheSceneInstancesAtTheirPosition) {
    ASSERT_TRUE(_runtime.run_string("load_scene('next')", "switch"));
    ASSERT_TRUE(_runtime.flush_scene().has_value());

    const std::optional<Entity> star = entity_of("star");
    ASSERT_TRUE(star.has_value());
    EXPECT_EQ(_world.get<Position>(star.value_or(Entity{0})).pos, (Vec2{5.0F, 6.0F}));
}

TEST_F(SceneSwitchTest, PersistentEntityKeepsItsLuaData) {
    const std::optional<Entity> keeper = entity_of("keeper");
    ASSERT_TRUE(keeper.has_value());
    ASSERT_TRUE(_runtime.call(keeper.value_or(Entity{0}), "bump"));

    ASSERT_TRUE(_runtime.run_string("load_scene('next')", "switch"));
    ASSERT_TRUE(_runtime.flush_scene().has_value());

    EXPECT_EQ(entity_of("keeper"), keeper);
    EXPECT_TRUE(_runtime.call(keeper.value_or(Entity{0}), "check_bumped"));
}

TEST_F(SceneSwitchTest, RunsOnDestroyThenOnSpawnThenOnSceneLoaded) {
    ASSERT_TRUE(_runtime.run_string(R"(
        trace = ""
        destroy_hook = function(type) trace = trace .. "destroy " .. type .. ";" end
        spawn_hook = function(type) trace = trace .. "spawn " .. type .. ";" end
        scene_hook = function(name) trace = trace .. "loaded " .. name .. ";" end
        load_scene("next")
    )",
                                    "switch"));

    ASSERT_TRUE(_runtime.flush_scene().has_value());

    EXPECT_TRUE(_runtime.run_string(
        R"(assert(trace == "destroy rock;spawn star;loaded next;", trace))", "check"));
}

TEST_F(SceneSwitchTest, QueuesTheBuiltInSceneLoadedEvent) {
    std::ignore = _server.take_event_commands();
    ASSERT_TRUE(_runtime.run_string("load_scene('next')", "switch"));

    ASSERT_TRUE(_runtime.flush_scene().has_value());

    EXPECT_EQ(_server.take_event_commands(),
              (std::vector<EventCommand>{
                  {.name = "scene_loaded", .arguments = {{.key = "name", .value = "next"}}}}));
}

TEST_F(SceneSwitchTest, FlushWithoutRequestDoesNothing) {
    EXPECT_EQ(_runtime.flush_scene(), std::nullopt);
    EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "rock"}));
}

TEST_F(SceneSwitchTest, LoadSceneCalledDuringTheSwitchIsAppliedByTheNextFlush) {
    ASSERT_TRUE(_runtime.run_string(R"(
        scene_hook = function(name)
            if name == "next" then load_scene("start") end
        end
        load_scene("next")
    )",
                                    "switch"));

    EXPECT_EQ(_runtime.flush_scene(), "next");
    EXPECT_EQ(_runtime.flush_scene(), "start");
    EXPECT_EQ(types(), (std::vector<std::string>{"keeper", "keeper", "rock"}));
}

TEST_F(SceneSwitchTest, SecondCallInOneTickIsALuaError) {
    ASSERT_TRUE(_runtime.run_string("load_scene('next')", "switch"));

    EXPECT_FALSE(_runtime.run_string("load_scene('start')", "switch"));

    EXPECT_TRUE(logged_error("cannot load 'start', scene 'next' is already pending"));
    EXPECT_EQ(_runtime.flush_scene(), "next");
}

TEST_F(SceneSwitchTest, UnknownSceneIsALuaErrorAndKeepsTheCurrentScene) {
    expect_rejected("load_scene('missing')", "cannot load scene 'missing'");
}

TEST_F(SceneSwitchTest, InvalidSceneFileIsALuaErrorAndKeepsTheCurrentScene) {
    expect_rejected("load_scene('broken')", "cannot load scene 'broken'");
}

TEST_F(SceneSwitchTest, InvalidSceneNameIsALuaError) {
    expect_rejected("load_scene('../start')", "'../start' is not a valid scene name");
    expect_rejected("load_scene(42)", "scene name must be a string");
}

TEST_F(SceneSwitchTest, RuntimeWithoutGameFolderCannotLoadScenes) {
    ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ASSERT_TRUE(runtime.load_scripts(switch_game() / "scripts"));

    EXPECT_FALSE(runtime.run_string("load_scene('next')", "switch"));

    EXPECT_TRUE(logged_error("no game folder is loaded"));
}

TEST_F(SceneSwitchTest, LoadSceneIsOnlyInTheServerApiSet) {
    ScriptRuntime runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_FALSE(runtime.run_string("load_scene('next')", "switch"));

    EXPECT_TRUE(logged_error("attempt to call a nil value (global 'load_scene')"));
}

TEST_F(SceneSwitchTest, PersistentIsNotAScriptField) {
    EXPECT_EQ(_runtime.script_fields("keeper"),
              (std::vector<ScriptField>{
                  {.name = "count", .default_value = engine::core::FieldValue{std::int64_t{0}}}}));
    EXPECT_TRUE(_runtime.script_fields("rock").empty());
}

TEST_F(SceneSwitchTest, PersistentMustBeABoolean) {
    ScriptRuntime runtime = ScriptRuntime::builder().with<ServerApi>().build();

    EXPECT_FALSE(
        runtime.load_scripts(std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "persistent"));

    EXPECT_TRUE(logged_error("entity 'anchor' field 'persistent' must be a boolean"));
    EXPECT_FALSE(runtime.find_declaration("anchor").has_value());
}
