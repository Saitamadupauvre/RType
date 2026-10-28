#include "WorldDraw.hpp"

#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"

namespace engine::app {

namespace {

constexpr float square_size = 64.F;

graphics::Color to_color(core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

} // namespace

WorldDraw::WorldDraw(graphics::IRenderer& renderer, std::filesystem::path assets)
    : _renderer(renderer), _assets(std::move(assets)) {}

void WorldDraw::draw(core::EntityManager& world) {
    world.query<core::Position, core::Sprite>().each(
        [&](core::Entity, const core::Position& position, const core::Sprite& sprite) {
            const auto [x, y] = position.pos;
            if (sprite.path.empty()) {
                _renderer.draw_rect({.x = x, .y = y, .width = square_size, .height = square_size},
                                    to_color(sprite.color));
                return;
            }
            const graphics::TextureId id = texture(sprite.path);
            const core::Vec2 size = _renderer.texture_size(id);
            _renderer.draw_sprite({.texture = id,
                                   .source = {.width = size.x, .height = size.y},
                                   .dest = {.x = x, .y = y, .width = size.x, .height = size.y},
                                   .tint = to_color(sprite.color)});
        });
}

graphics::TextureId WorldDraw::texture(const std::string& path) {
    const auto found = _textures.find(path);
    if (found != _textures.end()) {
        return found->second;
    }
    const graphics::TextureId id = _renderer.load_texture((_assets / path).string());
    _textures.emplace(path, id);
    return id;
}

} // namespace engine::app
