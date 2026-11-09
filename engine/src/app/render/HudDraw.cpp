#include "render/HudDraw.hpp"

#include <variant>

#include "render/SpriteLayout.hpp"

namespace engine::app {

HudDraw::HudDraw(graphics::IRenderer& renderer, TextureLibrary& textures)
    : _renderer(renderer), _textures(textures) {}

void HudDraw::draw(const std::vector<script::HudCommand>& commands) {
    for (const script::HudCommand& command : commands) {
        std::visit([this](const auto& shape) { draw_one(shape); }, command);
    }
}

void HudDraw::draw_one(const script::HudText& text) {
    _renderer.draw_text(text.text, text.position, text.size, to_color(text.color));
}

void HudDraw::draw_one(const script::HudRect& rect) {
    _renderer.draw_rect(
        {.x = rect.position.x, .y = rect.position.y, .width = rect.size.x, .height = rect.size.y},
        to_color(rect.color), rect.filled);
}

void HudDraw::draw_one(const script::HudSprite& sprite) {
    const graphics::TextureId id = _textures.get(sprite.texture);
    _renderer.draw_sprite(layout_sprite(id, _renderer.texture_size(id),
                                        {.top_left = sprite.position,
                                         .scale = sprite.scale,
                                         .rotation = sprite.rotation,
                                         .tint = sprite.color}));
}

} // namespace engine::app
