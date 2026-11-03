#include "engine/navigation/Pathfinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <queue>
#include <utility>

#include "engine/navigation/Smoothing.hpp"

namespace engine::navigation {

namespace {

constexpr std::array<std::pair<Heuristic, std::string_view>, 4> heuristic_names{{
    {Heuristic::Dijkstra, "dijkstra"},
    {Heuristic::Manhattan, "manhattan"},
    {Heuristic::Octile, "octile"},
    {Heuristic::Euclidean, "euclidean"},
}};

constexpr std::array<Cell, 4> straight_steps{
    {{.x = 1, .y = 0}, {.x = 0, .y = 1}, {.x = -1, .y = 0}, {.x = 0, .y = -1}}};
constexpr std::array<Cell, 4> diagonal_steps{
    {{.x = 1, .y = 1}, {.x = -1, .y = 1}, {.x = -1, .y = -1}, {.x = 1, .y = -1}}};

constexpr float infinite_cost = std::numeric_limits<float>::infinity();

Cell offset(Cell cell, Cell step) { return {.x = cell.x + step.x, .y = cell.y + step.y}; }

float estimate(Heuristic heuristic, Cell from, Cell to) {
    const auto dx = static_cast<float>(std::abs(from.x - to.x));
    const auto dy = static_cast<float>(std::abs(from.y - to.y));
    switch (heuristic) {
    case Heuristic::Manhattan:
        return dx + dy;
    case Heuristic::Octile:
        return std::max(dx, dy) + (std::numbers::sqrt2_v<float> - 1.0F) * std::min(dx, dy);
    case Heuristic::Euclidean:
        return std::hypot(dx, dy);
    case Heuristic::Dijkstra:
        break;
    }
    return 0.0F;
}

struct OpenEntry {
    float priority;
    float cost;
    std::size_t node;
};

struct ExpandsLater {
    bool operator()(const OpenEntry& left, const OpenEntry& right) const noexcept {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.cost < right.cost;
    }
};

class Search {
public:
    Search(const NavGrid& grid, Cell goal, const PathOptions& options)
        : _grid(grid), _goal(goal), _options(options),
          _costs(static_cast<std::size_t>(grid.width()) * static_cast<std::size_t>(grid.height()),
                 infinite_cost),
          _parents(_costs.size(), 0) {}

    PathResult run(Cell start) {
        PathResult result;
        open(start, 0.0F, node_of(start));
        while (!_open.empty()) {
            const OpenEntry entry = _open.top();
            _open.pop();
            if (entry.cost > _costs[entry.node]) {
                continue;
            }
            if (result.expanded == _options.max_expanded) {
                result.status = PathStatus::LimitReached;
                return result;
            }
            ++result.expanded;
            const Cell cell = cell_of(entry.node);
            if (cell == _goal) {
                result.status = PathStatus::Found;
                result.cost = entry.cost;
                result.cells = rebuild(start);
                return result;
            }
            relax_neighbours(cell);
        }
        result.status = PathStatus::Unreachable;
        return result;
    }

private:
    [[nodiscard]] std::size_t node_of(Cell cell) const {
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(_grid.width()) +
               static_cast<std::size_t>(cell.x);
    }

    [[nodiscard]] Cell cell_of(std::size_t node) const {
        const auto width = static_cast<std::size_t>(_grid.width());
        return {.x = static_cast<int>(node % width), .y = static_cast<int>(node / width)};
    }

    void open(Cell cell, float cost, std::size_t parent) {
        const std::size_t node = node_of(cell);
        _costs[node] = cost;
        _parents[node] = parent;
        _open.push({.priority = cost + estimate(_options.heuristic, cell, _goal),
                    .cost = cost,
                    .node = node});
    }

    void relax(Cell from, Cell to, float step_cost) {
        const float cost = _costs[node_of(from)] + step_cost;
        if (cost < _costs[node_of(to)]) {
            open(to, cost, node_of(from));
        }
    }

    void relax_neighbours(Cell cell) {
        for (const Cell step : straight_steps) {
            const Cell next = offset(cell, step);
            if (_grid.walkable(next)) {
                relax(cell, next, _grid.cost(next));
            }
        }
        if (!_options.diagonals) {
            return;
        }
        for (const Cell step : diagonal_steps) {
            const Cell next = offset(cell, step);
            const bool corner_free = _grid.walkable(offset(cell, {.x = step.x, .y = 0})) &&
                                     _grid.walkable(offset(cell, {.x = 0, .y = step.y}));
            if (corner_free && _grid.walkable(next)) {
                relax(cell, next, _grid.cost(next) * std::numbers::sqrt2_v<float>);
            }
        }
    }

    [[nodiscard]] std::vector<Cell> rebuild(Cell start) const {
        std::vector<Cell> cells;
        const std::size_t first = node_of(start);
        for (std::size_t node = node_of(_goal); node != first; node = _parents[node]) {
            cells.push_back(cell_of(node));
        }
        cells.push_back(start);
        std::ranges::reverse(cells);
        return cells;
    }

    const NavGrid& _grid;
    Cell _goal;
    const PathOptions& _options;
    std::vector<float> _costs;
    std::vector<std::size_t> _parents;
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, ExpandsLater> _open;
};

std::optional<PathStatus> rejected_endpoints(const NavGrid& grid, Cell start, Cell goal) {
    if (!grid.contains(start) || !grid.contains(goal)) {
        return PathStatus::OutsideGrid;
    }
    if (!grid.walkable(start) || !grid.walkable(goal)) {
        return PathStatus::Blocked;
    }
    return std::nullopt;
}

} // namespace

std::optional<Heuristic> parse_heuristic(std::string_view name) noexcept {
    for (const auto& [heuristic, candidate] : heuristic_names) {
        if (candidate == name) {
            return heuristic;
        }
    }
    return std::nullopt;
}

std::string_view name_of(Heuristic heuristic) noexcept {
    for (const auto& [candidate, name] : heuristic_names) {
        if (candidate == heuristic) {
            return name;
        }
    }
    return "unknown";
}

PathResult find_path(const NavGrid& grid, Cell start, Cell goal, const PathOptions& options) {
    if (const auto rejected = rejected_endpoints(grid, start, goal)) {
        return PathResult{.status = *rejected, .cells = {}, .cost = 0.0F, .expanded = 0};
    }
    PathResult result = Search(grid, goal, options).run(start);
    if (result.status == PathStatus::Found && options.smooth) {
        result.cells = smooth_path(grid, result.cells);
    }
    return result;
}

} // namespace engine::navigation
