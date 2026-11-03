#pragma once

#include <limits>
#include <span>
#include <vector>

#include "engine/navigation/Export.hpp"
#include "engine/navigation/NavGrid.hpp"

namespace engine::navigation {

/**
 * @brief Tells whether a straight line between two cell centres stays on walkable cells.
 *
 * Every cell the segment touches must be walkable with a cost of at most `max_cost`. When the
 * segment passes exactly through a cell corner, both cells beside that corner must be too, so the
 * line never squeezes between two blocked cells.
 *
 * @return false if either cell is outside the grid.
 */
[[nodiscard]] ENGINE_NAVIGATION_EXPORT bool
line_of_sight(const NavGrid& grid, Cell from, Cell to,
              float max_cost = std::numeric_limits<float>::infinity()) noexcept;

/**
 * @brief Shortens a path by skipping cells a straight line can cut across.
 *
 * Keeps the first and last cells, and from each kept cell jumps to the farthest following cell it
 * has a line_of_sight() to, never through a cell costlier than the most expensive cell of the
 * skipped part. A shortcut therefore never crosses a blocked cell or a costlier area the path
 * went around.
 *
 * @return The kept cells, in order. Paths of fewer than 3 cells are returned unchanged.
 */
[[nodiscard]] ENGINE_NAVIGATION_EXPORT std::vector<Cell> smooth_path(const NavGrid& grid,
                                                                     std::span<const Cell> path);

} // namespace engine::navigation
