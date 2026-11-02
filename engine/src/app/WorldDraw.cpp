#include "WorldDraw.hpp"

#include <cmath>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Transform.hpp"

namespace engine::app {

namespace {

constexpr float square_size = 64.F;

graphics::Color to_color(core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

core::Vec2 drawn_size(core::Vec2 size, core::Vec2 scale) {
    return {size.x * std::abs(scale.x), size.y * std::abs(scale.y)};
}

float mirrored(float length, float factor) { return std::signbit(factor) ? -length : length; }

} // namespace

WorldDraw::WorldDraw(graphics::IRenderer& renderer, const std::filesystem::path& assets)
    : _renderer(renderer),
      _textures(
          assets,
          [&renderer](const std::filesystem::path& path) {
              return renderer.load_texture(path.string());
          },
          [&renderer](graphics::TextureId texture) { renderer.unload_texture(texture); }) {}

bool WorldDraw::preload(const std::string& texture) {
    return _textures.get(texture) != graphics::TextureId{};
}

void WorldDraw::draw(core::EntityManager& world) {
    world.query<core::Position, core::Sprite>().each(
        [&](core::Entity entity, const core::Position& position, const core::Sprite& sprite) {
            const core::Vec2 scale = core::scale_of(world, entity);
            if (sprite.path.empty()) {
                draw_square(position.pos, scale, sprite.color);
                return;
            }
            draw_texture(position.pos, scale, core::rotation_of(world, entity), sprite);
        });
}

void WorldDraw::draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color) {
    const core::Vec2 size = drawn_size({square_size, square_size}, scale);
    _renderer.draw_rect({.x = position.x, .y = position.y, .width = size.x, .height = size.y},
                        to_color(color));
}

void WorldDraw::draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                             const core::Sprite& sprite) {
    const graphics::TextureId id = _textures.get(sprite.path);
    const core::Vec2 texture_size = _renderer.texture_size(id);
    const core::Vec2 size = drawn_size(texture_size, scale);
    const core::Vec2 center = size / 2.F;
    _renderer.draw_sprite({.texture = id,
                           .source = {.width = mirrored(texture_size.x, scale.x),
                                      .height = mirrored(texture_size.y, scale.y)},
                           .dest = {.x = position.x + center.x,
                                    .y = position.y + center.y,
                                    .width = size.x,
                                    .height = size.y},
                           .origin = center,
                           .rotation = rotation,
                           .tint = to_color(sprite.color)});
}

} // namespace engine::app
