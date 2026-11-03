#pragma once

#include "ShaderLibrary.hpp"
#include "TextureLibrary.hpp"
#include "engine/app/Export.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT WorldDraw {
public:
    WorldDraw(graphics::IRenderer& renderer, TextureLibrary& textures, ShaderLibrary& shaders);

    void draw(core::EntityManager& world, float seconds);

private:
    void draw_entity(core::EntityManager& world, core::Entity entity, core::Vec2 position,
                     const core::Sprite& sprite);
    [[nodiscard]] graphics::ShaderId shader_of(core::EntityManager& world, core::Entity entity);
    void begin_shader(graphics::ShaderId shader, float seconds);
    void draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color);
    void draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                      const core::Sprite& sprite);

    graphics::IRenderer& _renderer;
    TextureLibrary& _textures;
    ShaderLibrary& _shaders;
};

} // namespace engine::app
