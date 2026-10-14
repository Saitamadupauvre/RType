// TEMPORARY(#72): whole file. Placeholder drawing until the real renderer API lands.
// Delete with game/client/TempDraw.hpp and plugins/graphics-raylib/src/TempDraw.cpp.

#include "TempDraw.hpp"

#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"

namespace rtype::client {

namespace {

constexpr float square_size = 64.F;

} // namespace

TempDraw::TempDraw(const std::filesystem::path& plugin, std::filesystem::path assets)
    : _library(plugin), _assets(std::move(assets)) {
    if (_library) {
        _draw_sprite = _library.symbol<DrawSprite>("engine_temp_draw_sprite");
        _draw_rect = _library.symbol<DrawRect>("engine_temp_draw_rect");
    }
}

bool TempDraw::ready() const { return _draw_sprite != nullptr && _draw_rect != nullptr; }

void TempDraw::draw_world(engine::core::EntityManager& world) const {
    if (!ready()) {
        return;
    }
    world.query<engine::core::Position, engine::core::Sprite>().each(
        [&](engine::core::Entity, const engine::core::Position& position,
            const engine::core::Sprite& sprite) {
            const auto& color = sprite.color;
            if (sprite.path.empty()) {
                _draw_rect(position.pos.x, position.pos.y, square_size, square_size, color.r,
                           color.g, color.b, color.a);
                return;
            }
            _draw_sprite((_assets / sprite.path).string().c_str(), position.pos.x, position.pos.y,
                         color.r, color.g, color.b, color.a);
        });
}

} // namespace rtype::client
