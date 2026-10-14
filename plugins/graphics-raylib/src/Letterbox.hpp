#pragma once

#include "engine/graphics/Types.hpp"

inline constexpr engine::graphics::Vec2 virtual_resolution{.x = 1920.F, .y = 1080.F};

engine::graphics::Rect letterbox(engine::graphics::Vec2 window,
                                 engine::graphics::Vec2 virtual_size);

engine::graphics::Vec2 window_to_virtual(engine::graphics::Vec2 point,
                                         engine::graphics::Vec2 window,
                                         engine::graphics::Vec2 virtual_size);
