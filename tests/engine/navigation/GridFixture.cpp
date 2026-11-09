#include "GridFixture.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace navigation_test {

using engine::navigation::Cell;
using engine::navigation::NavGrid;

namespace {

std::vector<std::string> rows_of(std::string_view text) {
    std::vector<std::string> rows;
    std::istringstream stream{std::string(text)};
    for (std::string row; std::getline(stream, row);) {
        if (!row.empty()) {
            rows.push_back(row);
        }
    }
    if (rows.empty()) {
        throw std::invalid_argument("empty grid fixture");
    }
    return rows;
}

void apply_symbol(GridFixture& fixture, Cell cell, char symbol) {
    if (symbol == '#') {
        fixture.grid.set_blocked(cell, true);
    } else if (symbol >= '2' && symbol <= '9') {
        fixture.grid.set_cost(cell, static_cast<float>(symbol - '0'));
    } else if (symbol == 'S') {
        fixture.start = cell;
    } else if (symbol == 'G') {
        fixture.goal = cell;
    }
}

} // namespace

GridFixture parse_grid(std::string_view text) {
    const auto rows = rows_of(text);
    const auto width = static_cast<int>(rows.front().size());
    const auto height = static_cast<int>(rows.size());
    GridFixture fixture{.grid = NavGrid(width, height, 1.0F), .start = {}, .goal = {}};
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            apply_symbol(fixture, {.x = x, .y = y},
                         rows[static_cast<std::size_t>(y)].at(static_cast<std::size_t>(x)));
        }
    }
    return fixture;
}

GridFixture load_grid(const std::string& name) {
    std::ifstream file(std::string(TEST_FIXTURES_DIR) + "/navigation/" + name);
    if (!file) {
        throw std::runtime_error("missing grid fixture " + name);
    }
    std::ostringstream content;
    content << file.rdbuf();
    return parse_grid(content.str());
}

bool is_valid_grid_path(const NavGrid& grid, std::span<const Cell> path, bool diagonals) {
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (!grid.walkable(path[i])) {
            return false;
        }
        if (i == 0) {
            continue;
        }
        const int dx = path[i].x - path[i - 1].x;
        const int dy = path[i].y - path[i - 1].y;
        const int moved = std::abs(dx) + std::abs(dy);
        const bool diagonal = std::abs(dx) == 1 && std::abs(dy) == 1;
        if (moved == 0 || (moved > 1 && !diagonal) || (diagonal && !diagonals)) {
            return false;
        }
        if (diagonal && (!grid.walkable({.x = path[i - 1].x + dx, .y = path[i - 1].y}) ||
                         !grid.walkable({.x = path[i - 1].x, .y = path[i - 1].y + dy}))) {
            return false;
        }
    }
    return true;
}

bool touches_cost_above(const NavGrid& grid, std::span<const Cell> path, float cost) {
    for (const Cell cell : path) {
        if (grid.cost(cell) > cost) {
            return true;
        }
    }
    return false;
}

} // namespace navigation_test
