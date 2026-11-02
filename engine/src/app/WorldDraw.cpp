#include "WorldDraw.hpp"

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"

namespace engine::app {

namespace {

constexpr float square_size = 64.F;

graphics::Color to_color(core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

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
        [&](core::Entity, const core::Position& position, const core::Sprite& sprite) {
            const auto [x, y] = position.pos;
            if (sprite.path.empty()) {
                _renderer.draw_rect({.x = x, .y = y, .width = square_size, .height = square_size},
                                    to_color(sprite.color));
                return;
            }
            const graphics::TextureId id = _textures.get(sprite.path);
            const core::Vec2 size = _renderer.texture_size(id);
            _renderer.draw_sprite({.texture = id,
                                   .source = {.width = size.x, .height = size.y},
                                   .dest = {.x = x, .y = y, .width = size.x, .height = size.y},
                                   .tint = to_color(sprite.color)});
        });
}

} // namespace engine::app
