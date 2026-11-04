#pragma once

#include <clay.h>

namespace editor {

[[nodiscard]] Clay_BoundingBox intersect(Clay_BoundingBox first, Clay_BoundingBox second);

[[nodiscard]] float roundness(float radius, float width, float height);

} // namespace editor
