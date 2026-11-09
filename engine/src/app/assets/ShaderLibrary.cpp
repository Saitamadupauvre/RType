#include "assets/ShaderLibrary.hpp"

#include <system_error>

namespace engine::app {

namespace {

std::filesystem::path with_extension(const std::filesystem::path& base, const char* extension) {
    std::filesystem::path path = base;
    path += extension;
    return path;
}

bool is_file(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

} // namespace

ShaderFiles shader_files(const std::filesystem::path& base) {
    const std::filesystem::path vertex = with_extension(base, ".vs");
    return {.vertex = is_file(vertex) ? vertex : std::filesystem::path{},
            .fragment = with_extension(base, ".fs")};
}

ShaderLibrary::ShaderLibrary(graphics::IRenderer& renderer, const std::filesystem::path& assets)
    : _shaders(
          assets,
          [&renderer](const std::filesystem::path& base) {
              const ShaderFiles files = shader_files(base);
              return renderer.load_shader(files.vertex.string(), files.fragment.string());
          },
          [&renderer](graphics::ShaderId shader) { renderer.unload_shader(shader); }) {}

graphics::ShaderId ShaderLibrary::get(const std::string& name) { return _shaders.get(name); }

bool ShaderLibrary::preload(const std::string& name) {
    return _shaders.get(name) != graphics::ShaderId{};
}

void ShaderLibrary::clear() { _shaders.clear(); }

} // namespace engine::app
