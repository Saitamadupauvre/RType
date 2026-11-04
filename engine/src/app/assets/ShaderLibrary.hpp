#pragma once

#include <filesystem>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/core/util/AssetCache.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::app {

struct ShaderFiles {
    std::filesystem::path vertex;
    std::filesystem::path fragment;
};

[[nodiscard]] ENGINE_APP_EXPORT ShaderFiles shader_files(const std::filesystem::path& base);

class ENGINE_APP_EXPORT ShaderLibrary {
public:
    ShaderLibrary(graphics::IRenderer& renderer, const std::filesystem::path& assets);

    [[nodiscard]] graphics::ShaderId get(const std::string& name);
    bool preload(const std::string& name);
    void clear();

private:
    core::AssetCache<graphics::ShaderId> _shaders;
};

} // namespace engine::app
