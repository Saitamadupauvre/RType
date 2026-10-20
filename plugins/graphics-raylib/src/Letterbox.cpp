#include "Letterbox.hpp"

#include <algorithm>

engine::graphics::Rect letterbox(engine::core::Vec2 window, engine::core::Vec2 virtual_size) {
    const float scale = std::min(window.x / virtual_size.x, window.y / virtual_size.y);
    const float width = virtual_size.x * scale;
    const float height = virtual_size.y * scale;
    return {.x = (window.x - width) / 2.F,
            .y = (window.y - height) / 2.F,
            .width = width,
            .height = height};
}

engine::core::Vec2 window_to_virtual(engine::core::Vec2 point, engine::core::Vec2 window,
                                     engine::core::Vec2 virtual_size) {
    const engine::graphics::Rect view = letterbox(window, virtual_size);
    if (view.width <= 0.F || view.height <= 0.F) {
        return {};
    }
    return {(point.x - view.x) * virtual_size.x / view.width,
            (point.y - view.y) * virtual_size.y / view.height};
}
