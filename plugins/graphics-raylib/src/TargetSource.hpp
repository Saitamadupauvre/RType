#pragma once

#include <cmath>

#include "engine/core/Vec2.hpp"
#include "engine/graphics/Types.hpp"

inline engine::graphics::Rect upright_target_source(engine::graphics::Rect source,
                                                    engine::core::Vec2 texture_size) {
    return {.x = source.x,
            .y = texture_size.y - source.y - std::abs(source.height),
            .width = source.width,
            .height = -source.height};
}
