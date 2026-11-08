#include "viewport/Snap.hpp"

#include <cmath>

namespace editor {

namespace {

float snap_coordinate(float value, float grid) { return std::round(value / grid) * grid; }

} // namespace

engine::core::Vec2 snap_to_grid(engine::core::Vec2 point, float grid) {
    if (grid <= 0.F) {
        return point;
    }
    return {snap_coordinate(point.x, grid), snap_coordinate(point.y, grid)};
}

engine::core::Vec2 drop_position(engine::core::Vec2 point, float grid, bool snap, bool alt_held) {
    return snap && !alt_held ? snap_to_grid(point, grid) : point;
}

} // namespace editor
