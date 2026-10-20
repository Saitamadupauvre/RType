#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include "engine/core/EntityManager.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace rtype::client {

class WorldDraw {
public:
    WorldDraw(engine::graphics::IRenderer& renderer, std::filesystem::path assets);

    void draw(engine::core::EntityManager& world);

private:
    engine::graphics::TextureId texture(const std::string& path);

    engine::graphics::IRenderer& _renderer;
    std::filesystem::path _assets;
    std::unordered_map<std::string, engine::graphics::TextureId> _textures;
};

} // namespace rtype::client
