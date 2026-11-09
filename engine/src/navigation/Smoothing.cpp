#include "engine/navigation/Smoothing.hpp"

#include <algorithm>
#include <cstdlib>

namespace engine::navigation {

namespace {

bool passable(const NavGrid& grid, Cell cell, float max_cost) {
    return grid.walkable(cell) && grid.cost(cell) <= max_cost;
}

int sign_of(int value) { return value < 0 ? -1 : 1; }

} // namespace

bool line_of_sight(const NavGrid& grid, Cell from, Cell to, float max_cost) noexcept {
    if (!passable(grid, from, max_cost) || !passable(grid, to, max_cost)) {
        return false;
    }
    const int steps_x = std::abs(to.x - from.x);
    const int steps_y = std::abs(to.y - from.y);
    const int sign_x = sign_of(to.x - from.x);
    const int sign_y = sign_of(to.y - from.y);
    Cell current = from;
    int done_x = 0;
    int done_y = 0;
    while (done_x < steps_x || done_y < steps_y) {
        const long crossing = static_cast<long>(1 + 2 * done_x) * steps_y -
                              static_cast<long>(1 + 2 * done_y) * steps_x;
        if (crossing == 0) {
            if (!passable(grid, {.x = current.x + sign_x, .y = current.y}, max_cost) ||
                !passable(grid, {.x = current.x, .y = current.y + sign_y}, max_cost)) {
                return false;
            }
            current = {.x = current.x + sign_x, .y = current.y + sign_y};
            ++done_x;
            ++done_y;
        } else if (crossing < 0) {
            current.x += sign_x;
            ++done_x;
        } else {
            current.y += sign_y;
            ++done_y;
        }
        if (!passable(grid, current, max_cost)) {
            return false;
        }
    }
    return true;
}

std::vector<Cell> smooth_path(const NavGrid& grid, std::span<const Cell> path) {
    if (path.size() < 3) {
        return {path.begin(), path.end()};
    }
    std::vector<Cell> kept{path.front()};
    std::size_t anchor = 0;
    while (anchor + 1 < path.size()) {
        std::size_t reach = anchor + 1;
        float max_cost = std::max(grid.cost(path[anchor]), grid.cost(path[reach]));
        for (std::size_t next = reach + 1; next < path.size(); ++next) {
            max_cost = std::max(max_cost, grid.cost(path[next]));
            if (!line_of_sight(grid, path[anchor], path[next], max_cost)) {
                break;
            }
            reach = next;
        }
        kept.push_back(path[reach]);
        anchor = reach;
    }
    return kept;
}

} // namespace engine::navigation
