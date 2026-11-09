#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "GridFixture.hpp"
#include "engine/navigation/Pathfinding.hpp"
#include "engine/navigation/Smoothing.hpp"

using engine::navigation::Cell;
using engine::navigation::find_path;
using engine::navigation::Heuristic;
using engine::navigation::line_of_sight;
using engine::navigation::NavGrid;
using engine::navigation::PathStatus;
using engine::navigation::smooth_path;
using navigation_test::load_grid;
using navigation_test::parse_grid;

TEST(LineOfSight, SeesAcrossAnOpenGrid) {
    const NavGrid grid(6, 4, 1.0F);

    EXPECT_TRUE(line_of_sight(grid, {.x = 0, .y = 0}, {.x = 5, .y = 3}));
    EXPECT_TRUE(line_of_sight(grid, {.x = 5, .y = 3}, {.x = 0, .y = 0}));
    EXPECT_TRUE(line_of_sight(grid, {.x = 2, .y = 2}, {.x = 2, .y = 2}));
}

TEST(LineOfSight, IsBlockedByAWall) {
    const auto fixture = load_grid("wall.txt");

    EXPECT_FALSE(line_of_sight(fixture.grid, fixture.start, fixture.goal));
    EXPECT_TRUE(line_of_sight(fixture.grid, {.x = 0, .y = 0}, {.x = 9, .y = 0}));
}

TEST(LineOfSight, NeverSqueezesBetweenTwoTouchingCorners) {
    const auto fixture = parse_grid(".#\n#.\n");

    EXPECT_FALSE(line_of_sight(fixture.grid, {.x = 0, .y = 0}, {.x = 1, .y = 1}));
}

TEST(LineOfSight, RefusesCellsCostlierThanTheLimit) {
    const auto fixture = load_grid("swamp.txt");

    EXPECT_FALSE(line_of_sight(fixture.grid, fixture.start, fixture.goal, 1.0F));
    EXPECT_TRUE(line_of_sight(fixture.grid, fixture.start, fixture.goal, 9.0F));
}

TEST(LineOfSight, IsFalseOutsideTheGrid) {
    const NavGrid grid(2, 2, 1.0F);

    EXPECT_FALSE(line_of_sight(grid, {.x = 0, .y = 0}, {.x = 3, .y = 0}));
    EXPECT_FALSE(line_of_sight(grid, {.x = -1, .y = 0}, {.x = 1, .y = 0}));
}

TEST(SmoothPath, ReducesAnOpenPathToItsEndpoints) {
    const NavGrid grid(8, 8, 1.0F);
    const auto result = find_path(grid, {.x = 0, .y = 0}, {.x = 7, .y = 3});

    const auto smooth = smooth_path(grid, result.cells);

    EXPECT_EQ(smooth, (std::vector<Cell>{{.x = 0, .y = 0}, {.x = 7, .y = 3}}));
}

TEST(SmoothPath, NeverCrossesBlockedCells) {
    for (const std::string name : {"wall.txt", "maze.txt"}) {
        const auto fixture = load_grid(name);
        const auto result = find_path(fixture.grid, fixture.start, fixture.goal);
        ASSERT_EQ(result.status, PathStatus::Found);

        const auto smooth = smooth_path(fixture.grid, result.cells);

        ASSERT_GE(smooth.size(), 2U);
        for (std::size_t i = 1; i < smooth.size(); ++i) {
            EXPECT_TRUE(line_of_sight(fixture.grid, smooth[i - 1], smooth[i])) << name;
        }
    }
}

TEST(SmoothPath, DoesNotCutThroughCostlierCellsThePathAvoided) {
    const auto fixture = load_grid("swamp.txt");
    const auto result = find_path(fixture.grid, fixture.start, fixture.goal,
                                  {.heuristic = Heuristic::Octile,
                                   .diagonals = false,
                                   .max_expanded = 1000,
                                   .smooth = false});
    ASSERT_EQ(result.status, PathStatus::Found);

    const auto smooth = smooth_path(fixture.grid, result.cells);

    for (std::size_t i = 1; i < smooth.size(); ++i) {
        EXPECT_TRUE(line_of_sight(fixture.grid, smooth[i - 1], smooth[i], 1.0F));
    }
}

TEST(SmoothPath, LeavesShortPathsUnchanged) {
    const NavGrid grid(2, 2, 1.0F);
    const std::vector<Cell> path{{.x = 0, .y = 0}, {.x = 1, .y = 0}};

    EXPECT_EQ(smooth_path(grid, path), path);
    EXPECT_TRUE(smooth_path(grid, {}).empty());
}
