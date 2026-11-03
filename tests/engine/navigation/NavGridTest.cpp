#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

#include "engine/navigation/NavGrid.hpp"

using engine::navigation::Cell;
using engine::navigation::NavGrid;
using engine::navigation::Point;

TEST(NavGrid, StartsWalkableWithCostOne) {
    const NavGrid grid(4, 3, 10.0F);

    EXPECT_EQ(grid.width(), 4);
    EXPECT_EQ(grid.height(), 3);
    EXPECT_FLOAT_EQ(grid.cell_size(), 10.0F);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            EXPECT_TRUE(grid.walkable({.x = x, .y = y}));
            EXPECT_FLOAT_EQ(grid.cost({.x = x, .y = y}), 1.0F);
        }
    }
}

TEST(NavGrid, RejectsNonPositiveSizes) {
    EXPECT_THROW(NavGrid(0, 3, 1.0F), std::invalid_argument);
    EXPECT_THROW(NavGrid(3, -1, 1.0F), std::invalid_argument);
    EXPECT_THROW(NavGrid(3, 3, 0.0F), std::invalid_argument);
    EXPECT_THROW(NavGrid(3, 3, -2.0F), std::invalid_argument);
}

TEST(NavGrid, RejectsNonFiniteValues) {
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    EXPECT_THROW(NavGrid(3, 3, nan), std::invalid_argument);
    EXPECT_THROW(NavGrid(3, 3, inf), std::invalid_argument);
    EXPECT_THROW(NavGrid(3, 3, 1.0F, Point{.x = nan, .y = 0.0F}), std::invalid_argument);
}

TEST(NavGrid, RejectsTooManyCells) {
    EXPECT_THROW(NavGrid(1 << 11, 1 << 10, 1.0F), std::invalid_argument);
    EXPECT_NO_THROW(NavGrid(1 << 10, 1 << 10, 1.0F));
}

TEST(NavGrid, OutsideCellsAreNotWalkableAndCostNothing) {
    const NavGrid grid(2, 2, 1.0F);

    EXPECT_FALSE(grid.contains({.x = -1, .y = 0}));
    EXPECT_FALSE(grid.walkable({.x = 2, .y = 0}));
    EXPECT_FLOAT_EQ(grid.cost({.x = 0, .y = 2}), 0.0F);
}

TEST(NavGrid, BlockingKeepsTheCostForLater) {
    NavGrid grid(2, 2, 1.0F);
    grid.set_cost({.x = 1, .y = 1}, 4.0F);

    grid.set_blocked({.x = 1, .y = 1}, true);
    EXPECT_FALSE(grid.walkable({.x = 1, .y = 1}));
    EXPECT_FLOAT_EQ(grid.cost({.x = 1, .y = 1}), 0.0F);

    grid.set_blocked({.x = 1, .y = 1}, false);
    EXPECT_FLOAT_EQ(grid.cost({.x = 1, .y = 1}), 4.0F);
}

TEST(NavGrid, RejectsCellsOutside) {
    NavGrid grid(2, 2, 1.0F);

    EXPECT_THROW(grid.set_blocked({.x = 2, .y = 0}, true), std::out_of_range);
    EXPECT_THROW(grid.set_cost({.x = 0, .y = -1}, 2.0F), std::out_of_range);
}

TEST(NavGrid, RejectsCostsBelowOneOrNotFinite) {
    NavGrid grid(2, 2, 1.0F);

    EXPECT_THROW(grid.set_cost({.x = 0, .y = 0}, 0.5F), std::invalid_argument);
    EXPECT_THROW(grid.set_cost({.x = 0, .y = 0}, std::numeric_limits<float>::infinity()),
                 std::invalid_argument);
    EXPECT_THROW(grid.set_cost({.x = 0, .y = 0}, std::numeric_limits<float>::quiet_NaN()),
                 std::invalid_argument);
    EXPECT_FLOAT_EQ(grid.cost({.x = 0, .y = 0}), 1.0F);
}

TEST(NavGrid, MapsWorldPointsToCellsFromTheOrigin) {
    const NavGrid grid(4, 4, 10.0F, Point{.x = 100.0F, .y = 50.0F});

    EXPECT_EQ(grid.cell_at({.x = 100.0F, .y = 50.0F}), (Cell{.x = 0, .y = 0}));
    EXPECT_EQ(grid.cell_at({.x = 125.0F, .y = 89.9F}), (Cell{.x = 2, .y = 3}));
    EXPECT_EQ(grid.cell_at({.x = 99.9F, .y = 60.0F}), std::nullopt);
    EXPECT_EQ(grid.cell_at({.x = 140.0F, .y = 60.0F}), std::nullopt);
    EXPECT_EQ(grid.cell_at({.x = std::numeric_limits<float>::quiet_NaN(), .y = 60.0F}),
              std::nullopt);
    EXPECT_EQ(grid.center_of({.x = 2, .y = 1}), (Point{.x = 125.0F, .y = 65.0F}));
}

TEST(NavGrid, BlockAreaBlocksOnlyOverlappedCells) {
    NavGrid grid(5, 5, 10.0F);

    grid.block_area({.x = 10.0F, .y = 10.0F}, {.x = 25.0F, .y = 20.0F});

    EXPECT_FALSE(grid.walkable({.x = 1, .y = 1}));
    EXPECT_FALSE(grid.walkable({.x = 2, .y = 1}));
    EXPECT_TRUE(grid.walkable({.x = 0, .y = 1}));
    EXPECT_TRUE(grid.walkable({.x = 3, .y = 1}));
    EXPECT_TRUE(grid.walkable({.x = 1, .y = 2}));
    EXPECT_TRUE(grid.walkable({.x = 1, .y = 0}));
}

TEST(NavGrid, BlockAreaClipsToTheGridAndIgnoresEmptyAreas) {
    NavGrid grid(3, 3, 10.0F);

    grid.block_area({.x = -100.0F, .y = -100.0F}, {.x = 5.0F, .y = 5.0F});
    grid.block_area({.x = 20.0F, .y = 20.0F}, {.x = 20.0F, .y = 30.0F});
    grid.block_area({.x = 0.0F, .y = 0.0F},
                    {.x = std::numeric_limits<float>::infinity(), .y = 30.0F});

    EXPECT_FALSE(grid.walkable({.x = 0, .y = 0}));
    EXPECT_TRUE(grid.walkable({.x = 1, .y = 0}));
    EXPECT_TRUE(grid.walkable({.x = 2, .y = 2}));
}

TEST(NavGrid, ResetMakesEveryCellWalkableWithCostOne) {
    NavGrid grid(2, 1, 1.0F);
    grid.set_blocked({.x = 0, .y = 0}, true);
    grid.set_cost({.x = 1, .y = 0}, 3.0F);

    grid.reset();

    EXPECT_FLOAT_EQ(grid.cost({.x = 0, .y = 0}), 1.0F);
    EXPECT_FLOAT_EQ(grid.cost({.x = 1, .y = 0}), 1.0F);
}
