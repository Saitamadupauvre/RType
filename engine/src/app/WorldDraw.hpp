#pragma once

#include <filesystem>
#include <string>

#include "ShaderLibrary.hpp"
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

    void draw(core::EntityManager& world, float seconds);
    bool preload(const std::string& texture);
    bool preload_shader(const std::string& shader);

private:
    void draw_entity(core::EntityManager& world, core::Entity entity, core::Vec2 position,
                     const core::Sprite& sprite);
    [[nodiscard]] graphics::ShaderId shader_of(core::EntityManager& world, core::Entity entity);
    void begin_shader(graphics::ShaderId shader, float seconds);
    void draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color);
    void draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                      const core::Sprite& sprite);

    graphics::IRenderer& _renderer;
    core::AssetCache<graphics::TextureId> _textures;
    ShaderLibrary _shaders;
};

} // namespace engine::app
