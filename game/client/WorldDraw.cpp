#include "WorldDraw.hpp"

#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"

namespace rtype::client {

namespace {

constexpr float square_size = 64.F;

engine::graphics::Color to_color(engine::core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

} // namespace

WorldDraw::WorldDraw(engine::graphics::IRenderer& renderer, std::filesystem::path assets)
    : _renderer(renderer), _assets(std::move(assets)) {}

void WorldDraw::draw(engine::core::EntityManager& world) {
    world.query<engine::core::Position, engine::core::Sprite>().each(
        [&](engine::core::Entity, const engine::core::Position& position,
            const engine::core::Sprite& sprite) {
            const auto [x, y] = position.pos;
            if (sprite.path.empty()) {
                _renderer.draw_rect({.x = x, .y = y, .width = square_size, .height = square_size},
                                    to_color(sprite.color));
                return;
            }
            const engine::graphics::TextureId id = texture(sprite.path);
            const engine::core::Vec2 size = _renderer.texture_size(id);
            _renderer.draw_sprite({.texture = id,
                                   .source = {.width = size.x, .height = size.y},
                                   .dest = {.x = x, .y = y, .width = size.x, .height = size.y},
                                   .tint = to_color(sprite.color)});
        });
}

engine::graphics::TextureId WorldDraw::texture(const std::string& path) {
    const auto found = _textures.find(path);
    if (found != _textures.end()) {
        return found->second;
    }
    const engine::graphics::TextureId id = _renderer.load_texture((_assets / path).string());
    _textures.emplace(path, id);
    return id;
}

} // namespace rtype::client
