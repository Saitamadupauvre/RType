#pragma once

#include <cstdint>
#include <functional>
#include <initializer_list>
#include <vector>

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
    void draw(std::initializer_list<std::reference_wrapper<core::EntityManager>> worlds,
              float seconds);

private:
    struct Queued {
        std::int32_t z_index;
        core::EntityManager* world;
        core::Entity entity;
    };

    void queue(core::EntityManager& world);
    void draw_queued(const Queued& item, float seconds);
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
    std::vector<Queued> _queue;
};

} // namespace engine::app
