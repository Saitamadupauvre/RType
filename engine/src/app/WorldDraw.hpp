#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "engine/app/Export.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT WorldDraw {
public:
    WorldDraw(graphics::IRenderer& renderer, std::filesystem::path assets);

    void draw(core::EntityManager& world);

private:
    void draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color);
    void draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                      const core::Sprite& sprite);
    graphics::TextureId texture(const std::string& path);

    graphics::IRenderer& _renderer;
    std::filesystem::path _assets;
    std::unordered_map<std::string, graphics::TextureId> _textures;
};

} // namespace engine::app
