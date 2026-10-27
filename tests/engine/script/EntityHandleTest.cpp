#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::Velocity;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path entities_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "entities";
}

class EntityHandleTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime;

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(entities_fixture()));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] std::vector<Entity> living_entities() {
        std::vector<Entity> result;
        _world.query<Position>().each([&](Entity entity, Position&) { result.push_back(entity); });
        return result;
    }

    [[nodiscard]] Entity spawn_or_throw(const std::string& type,
                                        std::optional<Vec2> position = std::nullopt) {
        const std::optional<Entity> entity = _runtime.spawn(type, position);
        if (!entity.has_value()) {
            throw std::runtime_error("spawn failed: " + type);
        }
        return *entity;
    }

    [[nodiscard]] Entity only_entity() {
        const std::vector<Entity> entities = living_entities();
        EXPECT_EQ(entities.size(), 1U);
        return entities.front();
    }
};

} // namespace

TEST_F(EntityHandleTest, SpawnInitialisesComponentsFromPrefab) {
    const Entity bydo = spawn_or_throw("bydo");

    EXPECT_EQ(_world.get<Position>(bydo).pos, (Vec2{800.0F, 0.0F}));
    EXPECT_EQ(_world.get<Velocity>(bydo).vel, (Vec2{-150.0F, 0.0F}));
}

TEST_F(EntityHandleTest, SpawnPositionOverridesPrefab) {
    const Entity bydo = spawn_or_throw("bydo", Vec2{10.0F, 20.0F});

    EXPECT_EQ(_world.get<Position>(bydo).pos, (Vec2{10.0F, 20.0F}));
}

TEST_F(EntityHandleTest, ReadingNativeFieldReturnsComponent) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo")
        assert(bydo.x == 800 and bydo.y == 0 and bydo.vx == -150 and bydo.vy == 0)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, WritingNativeFieldUpdatesComponent) {
    ASSERT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo")
        bydo.x = 42
        bydo.vy = 7
    )",
                                    "test"));

    const Entity bydo = only_entity();
    EXPECT_EQ(_world.get<Position>(bydo).pos, (Vec2{42.0F, 0.0F}));
    EXPECT_EQ(_world.get<Velocity>(bydo).vel, (Vec2{-150.0F, 7.0F}));
}

TEST_F(EntityHandleTest, ReadOnlyFieldsDescribeTheEntity) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo")
        assert(bydo.type == "bydo")
        assert(math.type(bydo.id) == "integer")
        assert(bydo.id ~= spawn("bydo").id)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, CustomFieldsArePerEntity) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local first = spawn("bydo")
        local second = spawn("bydo")
        first.health = 10
        first.target = "player"
        assert(first.health == 10 and second.health == 2)
        assert(first.target == "player" and second.target == nil)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, PrefabMethodsReachableThroughHandle) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo")
        bydo:damage(1)
        assert(bydo.health == 1)
        assert(spawn("bydo").health == 2)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, SameEntityGivesSameHandle) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo")
        bydo.self_ref = bydo
        assert(rawequal(bydo.self_ref, bydo))
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, LuaSpawnAppliesOverrides) {
    ASSERT_TRUE(_runtime.run_string(R"(
        local bydo = spawn("bydo", { y = 5, health = 9 })
        assert(bydo.health == 9)
    )",
                                    "test"));

    EXPECT_EQ(_world.get<Position>(only_entity()).pos, (Vec2{800.0F, 5.0F}));
}

TEST_F(EntityHandleTest, CallRunsPrefabMethodWithHandle) {
    ASSERT_TRUE(_runtime.run_string("kept = spawn('bydo')", "test"));

    EXPECT_TRUE(_runtime.call(only_entity(), "heal"));
    EXPECT_TRUE(_runtime.run_string("assert(kept.health == 3)", "test"));
}

TEST_F(EntityHandleTest, CallReportsMissingMethodWithoutLogging) {
    const Entity bydo = spawn_or_throw("bydo");

    EXPECT_FALSE(_runtime.call(bydo, "on_spawn"));
    EXPECT_FALSE(logged_error("on_spawn"));
}

TEST_F(EntityHandleTest, CallLogsErrorRaisedByMethod) {
    const Entity bydo = spawn_or_throw("bydo");

    EXPECT_FALSE(_runtime.call(bydo, "explode"));
    EXPECT_TRUE(logged_error("bydo.lua:12: bydo exploded"));
}

TEST_F(EntityHandleTest, StaleHandleAfterKillLogsErrorAndDoesNotCrash) {
    ASSERT_TRUE(_runtime.run_string("kept = spawn('bydo')", "test"));
    _world.kill(only_entity());

    EXPECT_FALSE(_runtime.run_string("kept.x = 1", "stale"));
    EXPECT_FALSE(_runtime.run_string("return kept.health", "stale"));
    EXPECT_TRUE(logged_error("stale:1: attempt to use a destroyed entity"));
}

TEST_F(EntityHandleTest, StaleHandleAfterReleaseLogsError) {
    ASSERT_TRUE(_runtime.run_string("kept = spawn('bydo')", "test"));
    const Entity bydo = only_entity();
    _runtime.release(bydo);

    EXPECT_FALSE(_runtime.run_string("return kept.x", "stale"));
    EXPECT_TRUE(logged_error("attempt to use a destroyed entity"));
    EXPECT_FALSE(_runtime.call(bydo, "heal"));
}

TEST_F(EntityHandleTest, RecycledSlotDoesNotRevivePreviousHandle) {
    ASSERT_TRUE(_runtime.run_string("kept = spawn('bydo')", "test"));
    const Entity old = only_entity();
    _world.kill(old);
    _runtime.release(old);
    ASSERT_TRUE(_runtime.spawn("bydo"));

    EXPECT_FALSE(_runtime.run_string("return kept.x", "stale"));
}

TEST_F(EntityHandleTest, RejectsReadOnlyFieldWrite) {
    EXPECT_FALSE(_runtime.run_string("spawn('bydo').id = 3", "test"));
    EXPECT_TRUE(logged_error("entity field 'id' is read-only"));
    EXPECT_FALSE(_runtime.run_string("spawn('bydo').type = 'boss'", "test"));
    EXPECT_TRUE(logged_error("entity field 'type' is read-only"));
}

TEST_F(EntityHandleTest, RejectsWrongTypeForComponentField) {
    EXPECT_FALSE(_runtime.run_string("spawn('bydo').x = 'left'", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'x' cannot be set to a string"));
    EXPECT_FALSE(_runtime.run_string("spawn('bydo').vx = nil", "test"));
}

TEST_F(EntityHandleTest, LuaSpawnWithBadOverrideLeavesNoEntity) {
    EXPECT_FALSE(_runtime.run_string("spawn('bydo', { id = 1 })", "test"));
    EXPECT_TRUE(living_entities().empty());
}

TEST_F(EntityHandleTest, SpawnFailsForUnknownType) {
    EXPECT_FALSE(_runtime.spawn("bdyo"));
    EXPECT_TRUE(logged_error("cannot spawn unknown entity 'bdyo'"));
}

TEST_F(EntityHandleTest, LuaSpawnReturnsNilForUnknownType) {
    EXPECT_TRUE(_runtime.run_string("assert(spawn('bdyo') == nil)", "test"));
    EXPECT_TRUE(logged_error("test:1: cannot spawn unknown entity 'bdyo'"));
}

TEST_F(EntityHandleTest, SpawnFailsForSystem) {
    EXPECT_FALSE(_runtime.spawn("respawn"));
    EXPECT_TRUE(logged_error("it is declared as a system"));
}

TEST_F(EntityHandleTest, PrefabWithWrongNativeTypeFailsSpawn) {
    EXPECT_FALSE(_runtime.spawn("broken"));
    EXPECT_TRUE(logged_error("cannot spawn 'broken'"));
    EXPECT_TRUE(living_entities().empty());
}

TEST(EntityHandle, SpawnFailsWithoutWorld) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scope{sink, logger::Level::Trace};
    ScriptRuntime runtime;
    ASSERT_TRUE(runtime.load_scripts(entities_fixture()));

    EXPECT_FALSE(runtime.spawn("bydo"));
    EXPECT_TRUE(runtime.run_string("assert(spawn('bydo') == nil)", "test"));
}

TEST_F(EntityHandleTest, PrefabTableDefaultsAreNotSharedBetweenEntities) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local first = spawn('patrol')
        local second = spawn('patrol')
        first:forget_first()
        first.settings.nested.depth = 9
        assert(#first.waypoints == 1 and first.waypoints[1].x == 3)
        assert(#second.waypoints == 2 and second.waypoints[1].x == 1)
        assert(second.settings.nested.depth == 1)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, PrefabTableCopiesKeepCyclesInsideTheCopy) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local patrol = spawn('patrol')
        assert(patrol.settings.nested.owner == patrol.settings)
        assert(patrol.settings ~= spawn('patrol').settings)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, PrefabTableDefaultsStayIntactAfterInstancesChange) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local patrol = spawn('patrol')
        patrol.waypoints[1].x = 100
        assert(spawn('patrol').waypoints[1].x == 1)
    )",
                                    "test"));
}

TEST_F(EntityHandleTest, TooDeepPrefabTableFailsTheSpawn) {
    EXPECT_FALSE(_runtime.spawn("abyss"));
    EXPECT_TRUE(logged_error("cannot spawn 'abyss': prefab table nests deeper than 64 levels"));
    EXPECT_TRUE(living_entities().empty());
}
