#include "engine/navigation/NavGrid.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace engine::navigation {

namespace {

bool is_finite(Point point) { return std::isfinite(point.x) && std::isfinite(point.y); }

std::size_t cell_count(int width, int height, float cell_size, Point origin) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("navigation grid size must be positive");
    }
    if (!std::isfinite(cell_size) || cell_size <= 0.0F) {
        throw std::invalid_argument("navigation cell size must be positive and finite");
    }
    if (!is_finite(origin)) {
        throw std::invalid_argument("navigation grid origin must be finite");
    }
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (count > NavGrid::max_cells) {
        throw std::invalid_argument(std::format("navigation grid of {}x{} cells exceeds {} cells",
                                                width, height, NavGrid::max_cells));
    }
    return count;
}

int clamped_index(double value, int size) {
    return static_cast<int>(std::clamp(value, 0.0, static_cast<double>(size)));
}

} // namespace

NavGrid::NavGrid(int width, int height, float cell_size, Point origin)
    : _width(width), _height(height), _cell_size(cell_size), _origin(origin),
      _costs(cell_count(width, height, cell_size, origin), 1.0F), _blocked(_costs.size(), 0) {}

int NavGrid::width() const noexcept { return _width; }

int NavGrid::height() const noexcept { return _height; }

float NavGrid::cell_size() const noexcept { return _cell_size; }

Point NavGrid::origin() const noexcept { return _origin; }

bool NavGrid::contains(Cell cell) const noexcept {
    return cell.x >= 0 && cell.y >= 0 && cell.x < _width && cell.y < _height;
}

bool NavGrid::walkable(Cell cell) const noexcept {
    return contains(cell) && _blocked[index(cell)] == 0;
}

float NavGrid::cost(Cell cell) const noexcept {
    return walkable(cell) ? _costs[index(cell)] : 0.0F;
}

void NavGrid::set_blocked(Cell cell, bool blocked) {
    _blocked[checked_index(cell)] = blocked ? 1 : 0;
}

void NavGrid::set_cost(Cell cell, float cost) {
    const std::size_t at = checked_index(cell);
    if (!std::isfinite(cost) || cost < 1.0F) {
        throw std::invalid_argument("navigation cell cost must be finite and at least 1");
    }
    _costs[at] = cost;
}

void NavGrid::block_area(Point min, Point max) noexcept {
    if (!is_finite(min) || !is_finite(max) || max.x <= min.x || max.y <= min.y) {
        return;
    }
    const double size = _cell_size;
    const int first_x = clamped_index(std::floor((min.x - _origin.x) / size), _width);
    const int first_y = clamped_index(std::floor((min.y - _origin.y) / size), _height);
    const int end_x = clamped_index(std::ceil((max.x - _origin.x) / size), _width);
    const int end_y = clamped_index(std::ceil((max.y - _origin.y) / size), _height);
    for (int y = first_y; y < end_y; ++y) {
        for (int x = first_x; x < end_x; ++x) {
            _blocked[index({.x = x, .y = y})] = 1;
        }
    }
}

void NavGrid::reset() noexcept {
    std::ranges::fill(_costs, 1.0F);
    std::ranges::fill(_blocked, std::uint8_t{0});
}

std::optional<Cell> NavGrid::cell_at(Point point) const noexcept {
    if (!is_finite(point)) {
        return std::nullopt;
    }
    const double x = std::floor((point.x - _origin.x) / static_cast<double>(_cell_size));
    const double y = std::floor((point.y - _origin.y) / static_cast<double>(_cell_size));
    if (x < 0.0 || y < 0.0 || x >= _width || y >= _height) {
        return std::nullopt;
    }
    return Cell{.x = static_cast<int>(x), .y = static_cast<int>(y)};
}

Point NavGrid::center_of(Cell cell) const noexcept {
    return {.x = _origin.x + (static_cast<float>(cell.x) + 0.5F) * _cell_size,
            .y = _origin.y + (static_cast<float>(cell.y) + 0.5F) * _cell_size};
}

std::size_t NavGrid::index(Cell cell) const noexcept {
    return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(_width) +
           static_cast<std::size_t>(cell.x);
}

std::size_t NavGrid::checked_index(Cell cell) const {
    if (!contains(cell)) {
        throw std::out_of_range(
            std::format("cell ({}, {}) is outside the navigation grid", cell.x, cell.y));
    }
    return index(cell);
}

} // namespace engine::navigation
