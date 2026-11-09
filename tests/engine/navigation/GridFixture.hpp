#pragma once

#include <span>
#include <string>
#include <string_view>

#include "engine/navigation/NavGrid.hpp"

namespace navigation_test {

struct GridFixture {
    engine::navigation::NavGrid grid;
    engine::navigation::Cell start;
    engine::navigation::Cell goal;
};

GridFixture parse_grid(std::string_view text);
GridFixture load_grid(const std::string& name);
bool is_valid_grid_path(const engine::navigation::NavGrid& grid,
                        std::span<const engine::navigation::Cell> path, bool diagonals);
bool touches_cost_above(const engine::navigation::NavGrid& grid,
                        std::span<const engine::navigation::Cell> path, float cost);

} // namespace navigation_test
