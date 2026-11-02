#pragma once

#include <filesystem>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/core/AssetCache.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT WorldDraw {
public:
    WorldDraw(graphics::IRenderer& renderer, const std::filesystem::path& assets);

    void draw(core::EntityManager& world);
    bool preload(const std::string& texture);

private:
    graphics::IRenderer& _renderer;
    core::AssetCache<graphics::TextureId> _textures;
};

} // namespace engine::app
