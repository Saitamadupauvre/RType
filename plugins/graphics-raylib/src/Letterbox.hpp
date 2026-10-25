#pragma once

#include "engine/core/Vec2.hpp"
#include "engine/graphics/Types.hpp"

inline constexpr engine::core::Vec2 virtual_resolution{1920, 1080};

engine::graphics::Rect letterbox(engine::core::Vec2 window, engine::core::Vec2 virtual_size);

engine::core::Vec2 window_to_virtual(engine::core::Vec2 point, engine::core::Vec2 window,
                                     engine::core::Vec2 virtual_size);
