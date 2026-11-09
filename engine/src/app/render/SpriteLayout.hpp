#pragma once

#include "engine/app/Export.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::app {

struct SpritePlacement {
    core::Vec2 top_left;
    core::Vec2 scale;
    float rotation;
    core::Rgba tint;
};

[[nodiscard]] ENGINE_APP_EXPORT graphics::Color to_color(core::Rgba color);

[[nodiscard]] ENGINE_APP_EXPORT core::Vec2 scaled_size(core::Vec2 size, core::Vec2 scale);

[[nodiscard]] ENGINE_APP_EXPORT graphics::SpriteDraw
layout_sprite(graphics::TextureId texture, core::Vec2 texture_size,
              const SpritePlacement& placement);

} // namespace engine::app
