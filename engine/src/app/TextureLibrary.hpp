#pragma once

#include <filesystem>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/core/AssetCache.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT TextureLibrary {
public:
    TextureLibrary(graphics::IRenderer& renderer, const std::filesystem::path& assets);

    [[nodiscard]] graphics::TextureId get(const std::string& name);
    bool preload(const std::string& name);

private:
    core::AssetCache<graphics::TextureId> _textures;
};

} // namespace engine::app
