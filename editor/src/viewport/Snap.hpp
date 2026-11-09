#pragma once

#include "engine/core/math/Vec2.hpp"

namespace editor {

/** @brief Rounds each coordinate to the nearest multiple of `grid`; a grid of 0 or less keeps it.
 */
[[nodiscard]] engine::core::Vec2 snap_to_grid(engine::core::Vec2 point, float grid);

/**
 * @brief Where a dragged instance lands: snapped to the grid when snapping is on, unless Alt is
 * held for this drag.
 */
[[nodiscard]] engine::core::Vec2 drop_position(engine::core::Vec2 point, float grid, bool snap,
                                               bool alt_held);

} // namespace editor
