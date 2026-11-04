#include "render/Geometry.hpp"

#include <algorithm>

namespace editor {

Clay_BoundingBox intersect(Clay_BoundingBox first, Clay_BoundingBox second) {
    const float left = std::max(first.x, second.x);
    const float top = std::max(first.y, second.y);
    const float right = std::min(first.x + first.width, second.x + second.width);
    const float bottom = std::min(first.y + first.height, second.y + second.height);
    return {.x = left,
            .y = top,
            .width = std::max(0.F, right - left),
            .height = std::max(0.F, bottom - top)};
}

float roundness(float radius, float width, float height) {
    const float half_side = std::min(width, height) / 2.F;
    if (half_side <= 0.F) {
        return 0.F;
    }
    return std::clamp(radius / half_side, 0.F, 1.F);
}

} // namespace editor
