#include "Letterbox.hpp"

#include <algorithm>

engine::graphics::Rect letterbox(engine::graphics::Vec2 window,
                                 engine::graphics::Vec2 virtual_size) {
    const float scale = std::min(window.x / virtual_size.x, window.y / virtual_size.y);
    const float width = virtual_size.x * scale;
    const float height = virtual_size.y * scale;
    return {.x = (window.x - width) / 2.F,
            .y = (window.y - height) / 2.F,
            .width = width,
            .height = height};
}
