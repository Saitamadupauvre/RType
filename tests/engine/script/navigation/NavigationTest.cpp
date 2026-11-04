#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/physics/Components.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::physics::PhysicsBody;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

constexpr float tick = 1.0F / 60.0F;

constexpr std::string_view open_grid =
    "navgrid { width = 400, height = 200, cell = 20, obstacles = { 'wall' } }";

constexpr std::string_view path_helpers = R"(
    function inside_wall(point)
        return point.x > 180 and point.x < 220 and point.y < 160
    end

    function avoids_wall(path, from)
        local previous = from
        for _, point in ipairs(path) do
            for step = 0, 20 do
                local t = step / 20
                local sample = {
                    x = previous.x + (point.x - previous.x) * t,
                    y = previous.y + (point.y - previous.y) * t,
                }
                if inside_wall(sample) then
                    return false
                end
            end
            previous = point
        end
        return true
    end
)";

std::filesystem::path navigation_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "navigation";
}

class ScriptNavigationTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(navigation_fixture()));
        ASSERT_TRUE(lua(path_helpers));
    }

    bool lua(std::string_view code) { return _runtime.run_string(code, "test"); }

    void step() {
        _runtime.update(tick);
        _world.query<Position, PhysicsBody>().each(
            [](Entity, Position& position, PhysicsBody& body) {
                position.pos += body.velocity * tick;
            });
        _runtime.flush_destroyed();
    }

    void run_ticks(int count) {
        for (int i = 0; i < count; ++i) {
            step();
        }
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const logger::MemoryRecord& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(ScriptNavigationTest, FindPathNeedsANavgrid) {
    EXPECT_FALSE(lua("find_path({ x = 50, y = 50 }, { x = 350, y = 50 })"));
    EXPECT_TRUE(logged_error("find_path: call navgrid { ... } first"));
}

TEST_F(ScriptNavigationTest, NavgridRejectsBadOptions) {
    const auto cases = std::to_array<std::pair<std::string_view, std::string_view>>({
        {"navgrid()", "options must be a table"},
        {"navgrid { width = 100 }", "'height' is required"},
        {"navgrid { width = 100, height = 100, cell = 0 }", "'cell' must be positive"},
        {"navgrid { width = 100, height = 100, colour = 1 }", "unknown option 'colour'"},
        {"navgrid { width = 100, height = 100, heuristic = 'astar' }", "'heuristic' must be"},
        {"navgrid { width = 1e9, height = 1e9 }", "exceeds"},
        {"navgrid { width = 100, height = 100, obstacles = 'wall' }", "array of tags"},
        {"navgrid { width = 100, height = 100, max_nodes = 1.5 }", "whole number"},
        {"navgrid { width = 100, height = 100, padding = -1 }", "must not be negative"},
        {"navgrid { width = 100, height = 100, diagonals = 1 }", "must be a boolean"},
        {"navgrid { width = 0/0, height = 100 }", "finite number"},
    });
    for (const auto& [code, message] : cases) {
        EXPECT_FALSE(lua(code)) << code;
        EXPECT_TRUE(logged_error(std::string(message))) << code;
    }
}

TEST_F(ScriptNavigationTest, FailedNavgridKeepsThePreviousGrid) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_FALSE(lua("navgrid { width = -1, height = 100 }"));

    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 350, y = 50 }))"));
}

TEST_F(ScriptNavigationTest, FindsAStraightPathOnAnOpenGrid) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(R"(
        local path = find_path({ x = 50, y = 50 }, { x = 350, y = 50 })
        assert(#path == 1, #path)
        assert(path[1].x == 350 and path[1].y == 50)
    )"));
}

TEST_F(ScriptNavigationTest, ReturnsEveryCellCentreWithoutSmoothing) {
    ASSERT_TRUE(lua("navgrid { width = 400, height = 200, cell = 20, smooth = false }"));

    EXPECT_TRUE(lua(R"(
        local path = find_path({ x = 50, y = 50 }, { x = 353, y = 52 })
        assert(#path == 15, #path)
        assert(path[1].x == 70 and path[1].y == 50)
        assert(path[15].x == 353 and path[15].y == 52)
    )"));
}

TEST_F(ScriptNavigationTest, GoesAroundTaggedObstacles) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(R"(
        spawn("wall", { x = 180, y = 0 })
        local from = { x = 50, y = 50 }
        local path = find_path(from, { x = 350, y = 50 })
        assert(#path > 1)
        assert(avoids_wall(path, from))
    )"));
}

TEST_F(ScriptNavigationTest, BlocksTheCellsObstacleHitboxesOverlap) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(spawn("wall", { x = 180, y = 0 }))"));

    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 50 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 230, y = 50 }))"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 170, y = 50 }))"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 170 }))"));
}

TEST_F(ScriptNavigationTest, PaddingGrowsObstacles) {
    ASSERT_TRUE(lua(
        "navgrid { width = 400, height = 200, cell = 20, obstacles = { 'wall' }, padding = 10 }"));
    ASSERT_TRUE(lua(R"(spawn("wall", { x = 180, y = 0 }))"));

    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 230, y = 50 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 170 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 250, y = 50 }))"));
}

TEST_F(ScriptNavigationTest, AvoidOptionAddsObstaclesForOneCall) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(spawn("hazard", { x = 180, y = 30 }))"));

    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 190, y = 50 }))"));
    EXPECT_TRUE(lua(
        "assert(find_path({ x = 50, y = 50 }, { x = 190, y = 50 }, { avoid = { 'hazard' } }) == "
        "nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 190, y = 50 }))"));
}

TEST_F(ScriptNavigationTest, ReturnsNilWhenNoPathExists) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(
        spawn("wall", { x = 180, y = 0 })
        spawn("wall", { x = 180, y = 100 })
    )"));

    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 350, y = 50 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 500, y = 50 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = -5, y = 50 }, { x = 100, y = 50 }) == nil)"));
}

TEST_F(ScriptNavigationTest, ReturnsNilWhenTheSearchLimitIsReached) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(
        "assert(find_path({ x = 50, y = 50 }, { x = 350, y = 50 }, { max_nodes = 2 }) == nil)"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 350, y = 50 }))"));
}

TEST_F(ScriptNavigationTest, AcceptsEntitiesAsEndpoints) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(R"(
        local walker = spawn("walker", { x = 45, y = 45 })
        local target = spawn("walker", { x = 345, y = 45 })
        local path = find_path(walker, target)
        assert(#path == 1)
        assert(path[1].x == 350 and path[1].y == 50)
    )"));
}

TEST_F(ScriptNavigationTest, RejectsBadArguments) {
    ASSERT_TRUE(lua(open_grid));
    const auto cases = std::to_array<std::pair<std::string_view, std::string_view>>({
        {"find_path(3, { x = 1, y = 1 })", "'from' must be an entity or a table"},
        {"find_path({ x = 1, y = 1 })", "'to' must be an entity or a table"},
        {"find_path({ x = 1 }, { x = 1, y = 1 })", "'from' must be"},
        {"find_path({ x = 0/0, y = 1 }, { x = 1, y = 1 })", "'from' must be"},
        {"find_path({ x = 1, y = 1 }, { x = 2, y = 2 }, { colour = 1 })", "unknown option"},
        {"find_path({ x = 1, y = 1 }, { x = 2, y = 2 }, { max_nodes = 0 })", "whole number"},
        {"find_path({ x = 1, y = 1 }, { x = 2, y = 2 }, { avoid = { 1 } })", "array of tags"},
        {"find_path({ x = 1, y = 1 }, { x = 2, y = 2 }, 5)", "options must be a table"},
    });
    for (const auto& [code, message] : cases) {
        EXPECT_FALSE(lua(code)) << code;
        EXPECT_TRUE(logged_error(std::string(message))) << code;
    }
}

TEST_F(ScriptNavigationTest, SeesObstaclesFromTheNextTickOn) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 50 }))"));

    ASSERT_TRUE(lua(R"(spawn("wall", { x = 180, y = 0 }))"));
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 50 }))"));

    _runtime.update(tick);
    EXPECT_TRUE(lua("assert(find_path({ x = 50, y = 50 }, { x = 200, y = 50 }) == nil)"));
}

TEST_F(ScriptNavigationTest, AgentWalksToItsTargetAndArrives) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(
        walker = spawn("walker", { x = 45, y = 45 })
        assert(walker:navigate_to({ x = 350, y = 50 }, 300))
        assert(walker:is_navigating())
    )"));

    step();
    EXPECT_TRUE(lua("assert(math.abs(walker.vx - 300) < 0.01 and math.abs(walker.vy) < 0.01)"));

    run_ticks(120);

    EXPECT_TRUE(lua(R"(
        assert(walker.arrived)
        assert(not walker:is_navigating())
        assert(math.abs(walker.x - 345) < 0.1 and math.abs(walker.y - 45) < 0.1)
        assert(walker.vx == 0 and walker.vy == 0)
    )"));
}

TEST_F(ScriptNavigationTest, AgentWalksAroundObstacles) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(
        spawn("wall", { x = 180, y = 0 })
        walker = spawn("walker", { x = 45, y = 45 })
        assert(walker:navigate_to({ x = 350, y = 50 }, 200))
    )"));

    for (int i = 0; i < 300; ++i) {
        step();
        ASSERT_TRUE(lua("assert(not inside_wall({ x = walker.x + 5, y = walker.y + 5 }))")) << i;
    }

    EXPECT_TRUE(lua("assert(walker.arrived)"));
}

TEST_F(ScriptNavigationTest, AgentWithoutABodyGetsOne) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(R"(
        ghost = spawn("ghost", { x = 50, y = 50 })
        assert(ghost.vx == nil)
        assert(ghost:navigate_to({ x = 350, y = 50 }, 100))
    )"));
    step();

    EXPECT_TRUE(lua("assert(math.abs(ghost.vx - 100) < 0.01)"));
}

TEST_F(ScriptNavigationTest, StopNavigationHaltsTheAgent) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(
        walker = spawn("walker", { x = 45, y = 45 })
        walker:navigate_to({ x = 350, y = 50 }, 100)
    )"));
    step();

    EXPECT_TRUE(lua(R"(
        walker:stop_navigation()
        assert(not walker:is_navigating())
        assert(walker.vx == 0 and walker.vy == 0)
    )"));
    run_ticks(5);
    EXPECT_TRUE(lua("assert(walker.vx == 0 and not walker.arrived)"));
}

TEST_F(ScriptNavigationTest, NavigateToReturnsFalseWithoutAPath) {
    ASSERT_TRUE(lua(open_grid));

    EXPECT_TRUE(lua(R"(
        spawn("wall", { x = 180, y = 0 })
        spawn("wall", { x = 180, y = 100 })
        local walker = spawn("walker", { x = 45, y = 45 })
        assert(walker:navigate_to({ x = 350, y = 50 }, 100) == false)
        assert(not walker:is_navigating())
    )"));
}

TEST_F(ScriptNavigationTest, NavigateToRejectsBadArguments) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(walker = spawn("walker", { x = 45, y = 45 }))"));

    EXPECT_FALSE(lua("walker:navigate_to({ x = 350, y = 50 }, 0)"));
    EXPECT_TRUE(logged_error("navigate_to: speed must be a positive number"));
    EXPECT_FALSE(lua("walker:navigate_to({ x = 350, y = 50 }, 'fast')"));
    EXPECT_FALSE(lua("walker:navigate_to('there', 100)"));
    EXPECT_TRUE(logged_error("navigate_to: 'to' must be an entity or a table"));
}

TEST_F(ScriptNavigationTest, ForgetsDestroyedAgents) {
    ASSERT_TRUE(lua(open_grid));
    ASSERT_TRUE(lua(R"(
        local walker = spawn("walker", { x = 45, y = 45 })
        walker:navigate_to({ x = 350, y = 50 }, 100)
        walker:destroy()
    )"));

    run_ticks(3);

    EXPECT_FALSE(logged_error(""));
}
