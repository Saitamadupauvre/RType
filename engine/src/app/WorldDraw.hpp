#pragma once

#include <filesystem>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/core/AssetCache.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT WorldDraw {
public:
    WorldDraw(graphics::IRenderer& renderer, const std::filesystem::path& assets);

    void draw(core::EntityManager& world);
    bool preload(const std::string& texture);

private:
    void draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color);
    void draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                      const core::Sprite& sprite);

    graphics::IRenderer& _renderer;
    core::AssetCache<graphics::TextureId> _textures;
};

} // namespace engine::app
