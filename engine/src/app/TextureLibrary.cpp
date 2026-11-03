#include "TextureLibrary.hpp"

namespace engine::app {

TextureLibrary::TextureLibrary(graphics::IRenderer& renderer, const std::filesystem::path& assets)
    : _textures(
          assets,
          [&renderer](const std::filesystem::path& path) {
              return renderer.load_texture(path.string());
          },
          [&renderer](graphics::TextureId texture) { renderer.unload_texture(texture); }) {}

graphics::TextureId TextureLibrary::get(const std::string& name) { return _textures.get(name); }

bool TextureLibrary::preload(const std::string& name) {
    return _textures.get(name) != graphics::TextureId{};
}

} // namespace engine::app
