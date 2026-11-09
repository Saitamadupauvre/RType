#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <numbers>
#include <string>

#include "GridFixture.hpp"
#include "engine/navigation/Pathfinding.hpp"

using engine::navigation::Cell;
using engine::navigation::find_path;
using engine::navigation::Heuristic;
using engine::navigation::NavGrid;
using engine::navigation::PathOptions;
using engine::navigation::PathStatus;
using navigation_test::is_valid_grid_path;
using navigation_test::load_grid;

namespace {

constexpr std::array admissible{Heuristic::Dijkstra, Heuristic::Octile, Heuristic::Euclidean};

PathOptions options_with(Heuristic heuristic, bool diagonals) {
    return PathOptions{.heuristic = heuristic, .diagonals = diagonals};
}

} // namespace

TEST(Pathfinding, FindsTheStraightPathOnAnOpenGrid) {
    const auto fixture = load_grid("open.txt");

    const auto result =
        find_path(fixture.grid, fixture.start, fixture.goal, options_with(Heuristic::Octile, true));

    ASSERT_EQ(result.status, PathStatus::Found);
    EXPECT_FLOAT_EQ(result.cost, 9.0F);
    EXPECT_EQ(result.cells.size(), 10U);
    EXPECT_EQ(result.cells.front(), fixture.start);
    EXPECT_EQ(result.cells.back(), fixture.goal);
}

TEST(Pathfinding, EveryAdmissibleHeuristicFindsTheDijkstraCost) {
    for (const std::string name : {"open.txt", "wall.txt", "swamp.txt", "maze.txt"}) {
        const auto fixture = load_grid(name);
        for (const bool diagonals : {false, true}) {
            const auto reference = find_path(fixture.grid, fixture.start, fixture.goal,
                                             options_with(Heuristic::Dijkstra, diagonals));
            ASSERT_EQ(reference.status, PathStatus::Found) << name;
            for (const Heuristic heuristic : admissible) {
                const auto result = find_path(fixture.grid, fixture.start, fixture.goal,
                                              options_with(heuristic, diagonals));
                ASSERT_EQ(result.status, PathStatus::Found) << name;
                EXPECT_NEAR(result.cost, reference.cost, 1e-4F) << name;
                EXPECT_TRUE(is_valid_grid_path(fixture.grid, result.cells, diagonals)) << name;
            }
        }
    }
}

TEST(Pathfinding, ManhattanIsExactWithoutDiagonals) {
    const auto fixture = load_grid("wall.txt");

    const auto dijkstra = find_path(fixture.grid, fixture.start, fixture.goal,
                                    options_with(Heuristic::Dijkstra, false));
    const auto manhattan = find_path(fixture.grid, fixture.start, fixture.goal,
                                     options_with(Heuristic::Manhattan, false));

    ASSERT_EQ(manhattan.status, PathStatus::Found);
    EXPECT_FLOAT_EQ(manhattan.cost, dijkstra.cost);
}

TEST(Pathfinding, HeuristicsExpandFewerCellsThanDijkstra) {
    const auto fixture = load_grid("open.txt");

    const auto dijkstra = find_path(fixture.grid, fixture.start, fixture.goal,
                                    options_with(Heuristic::Dijkstra, true));
    const auto octile =
        find_path(fixture.grid, fixture.start, fixture.goal, options_with(Heuristic::Octile, true));

    EXPECT_LT(octile.expanded, dijkstra.expanded);
    EXPECT_EQ(octile.expanded, 10U);
}

TEST(Pathfinding, GoesAroundCostlyCellsWhenCheaper) {
    const auto fixture = load_grid("swamp.txt");

    const auto result = find_path(fixture.grid, fixture.start, fixture.goal,
                                  options_with(Heuristic::Octile, false));

    ASSERT_EQ(result.status, PathStatus::Found);
    EXPECT_FALSE(navigation_test::touches_cost_above(fixture.grid, result.cells, 1.0F));
    EXPECT_FLOAT_EQ(result.cost, 13.0F);
}

TEST(Pathfinding, DiagonalMovesCostTheSquareRootOfTwo) {
    const NavGrid grid(3, 3, 1.0F);

    const auto result = find_path(grid, {.x = 0, .y = 0}, {.x = 2, .y = 2});

    ASSERT_EQ(result.status, PathStatus::Found);
    EXPECT_FLOAT_EQ(result.cost, 2.0F * std::numbers::sqrt2_v<float>);
    EXPECT_EQ(result.cells.size(), 3U);
}

TEST(Pathfinding, NeverCutsWallCorners) {
    const auto fixture = load_grid("corner.txt");

    const auto result =
        find_path(fixture.grid, fixture.start, fixture.goal, options_with(Heuristic::Octile, true));

    EXPECT_EQ(result.status, PathStatus::Unreachable);
    EXPECT_TRUE(result.cells.empty());
}

TEST(Pathfinding, ReportsAnUnreachableGoal) {
    const auto fixture = load_grid("unreachable.txt");

    const auto result = find_path(fixture.grid, fixture.start, fixture.goal);

    EXPECT_EQ(result.status, PathStatus::Unreachable);
    EXPECT_TRUE(result.cells.empty());
    EXPECT_FLOAT_EQ(result.cost, 0.0F);
    EXPECT_EQ(result.expanded, 16U);
}

TEST(Pathfinding, RejectsABlockedStartOrGoal) {
    NavGrid grid(3, 1, 1.0F);
    grid.set_blocked({.x = 0, .y = 0}, true);

    EXPECT_EQ(find_path(grid, {.x = 0, .y = 0}, {.x = 2, .y = 0}).status, PathStatus::Blocked);
    EXPECT_EQ(find_path(grid, {.x = 2, .y = 0}, {.x = 0, .y = 0}).status, PathStatus::Blocked);
}

TEST(Pathfinding, RejectsEndpointsOutsideTheGrid) {
    const NavGrid grid(3, 1, 1.0F);

    EXPECT_EQ(find_path(grid, {.x = -1, .y = 0}, {.x = 2, .y = 0}).status, PathStatus::OutsideGrid);
    EXPECT_EQ(find_path(grid, {.x = 0, .y = 0}, {.x = 0, .y = 1}).status, PathStatus::OutsideGrid);
}

TEST(Pathfinding, StopsWhenTheExpansionLimitIsReached) {
    const auto fixture = load_grid("open.txt");
    PathOptions options;
    options.max_expanded = 4;

    const auto result = find_path(fixture.grid, fixture.start, fixture.goal, options);

    EXPECT_EQ(result.status, PathStatus::LimitReached);
    EXPECT_EQ(result.expanded, 4U);
    EXPECT_TRUE(result.cells.empty());
}

TEST(Pathfinding, SameStartAndGoalGiveAOneCellPath) {
    const NavGrid grid(3, 3, 1.0F);

    const auto result = find_path(grid, {.x = 1, .y = 1}, {.x = 1, .y = 1});

    ASSERT_EQ(result.status, PathStatus::Found);
    EXPECT_EQ(result.cells, (std::vector<Cell>{{.x = 1, .y = 1}}));
    EXPECT_FLOAT_EQ(result.cost, 0.0F);
}

TEST(Pathfinding, SmoothOptionKeepsTheEndpointsAndTheGridCost) {
    const auto fixture = load_grid("wall.txt");
    PathOptions options;
    options.smooth = true;

    const auto raw = find_path(fixture.grid, fixture.start, fixture.goal);
    const auto smooth = find_path(fixture.grid, fixture.start, fixture.goal, options);

    ASSERT_EQ(smooth.status, PathStatus::Found);
    EXPECT_LT(smooth.cells.size(), raw.cells.size());
    EXPECT_EQ(smooth.cells.front(), fixture.start);
    EXPECT_EQ(smooth.cells.back(), fixture.goal);
    EXPECT_FLOAT_EQ(smooth.cost, raw.cost);
}

TEST(Pathfinding, IsDeterministic) {
    const auto fixture = load_grid("maze.txt");

    const auto first = find_path(fixture.grid, fixture.start, fixture.goal);
    const auto second = find_path(fixture.grid, fixture.start, fixture.goal);

    EXPECT_EQ(first.cells, second.cells);
    EXPECT_EQ(first.expanded, second.expanded);
}

TEST(Heuristic, ParsesItsNames) {
    using engine::navigation::name_of;
    using engine::navigation::parse_heuristic;

    for (const Heuristic heuristic :
         {Heuristic::Dijkstra, Heuristic::Manhattan, Heuristic::Octile, Heuristic::Euclidean}) {
        EXPECT_EQ(parse_heuristic(name_of(heuristic)), heuristic);
    }
    EXPECT_EQ(parse_heuristic("Octile"), std::nullopt);
    EXPECT_EQ(parse_heuristic(""), std::nullopt);
}
