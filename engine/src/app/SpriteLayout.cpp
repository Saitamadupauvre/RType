#include "SpriteLayout.hpp"

#include <cmath>

namespace engine::app {

namespace {

float mirrored(float length, float factor) { return std::signbit(factor) ? -length : length; }

} // namespace

graphics::Color to_color(core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

core::Vec2 scaled_size(core::Vec2 size, core::Vec2 scale) {
    return {size.x * std::abs(scale.x), size.y * std::abs(scale.y)};
}

graphics::SpriteDraw layout_sprite(graphics::TextureId texture, core::Vec2 texture_size,
                                   const SpritePlacement& placement) {
    const core::Vec2 size = scaled_size(texture_size, placement.scale);
    const core::Vec2 center = size / 2.F;
    return {.texture = texture,
            .source = {.width = mirrored(texture_size.x, placement.scale.x),
                       .height = mirrored(texture_size.y, placement.scale.y)},
            .dest = {.x = placement.top_left.x + center.x,
                     .y = placement.top_left.y + center.y,
                     .width = size.x,
                     .height = size.y},
            .origin = center,
            .rotation = placement.rotation,
            .tint = to_color(placement.tint)};
}

} // namespace engine::app
