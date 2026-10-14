#include "engine/graphics/PluginLoader.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace engine::graphics {

namespace {

PluginLoadResult rejected(const std::filesystem::path& path, const std::string& reason) {
    return {.plugin = std::nullopt, .error = "graphics plugin " + path.string() + ": " + reason};
}

std::string missing(std::string_view symbol) {
    return "missing entry point " + std::string(symbol);
}

} // namespace

PluginLoadResult load_graphics_plugin(const std::filesystem::path& path) {
    core::DynamicLibrary library(path);
    if (!library) {
        return {.plugin = std::nullopt, .error = library.error()};
    }

    const auto api_version =
        library.symbol<ApiVersionFunction>(engine::graphics::api_version_symbol);
    if (api_version == nullptr) {
        return rejected(path, missing(engine::graphics::api_version_symbol));
    }
    const auto create = library.symbol<CreateFunction>(engine::graphics::create_symbol);
    if (create == nullptr) {
        return rejected(path, missing(engine::graphics::create_symbol));
    }
    const auto destroy = library.symbol<DestroyFunction>(engine::graphics::destroy_symbol);
    if (destroy == nullptr) {
        return rejected(path, missing(engine::graphics::destroy_symbol));
    }

    const std::uint32_t version = api_version();
    if (version != plugin_api_version) {
        return rejected(path, "API version " + std::to_string(version) + ", engine expects " +
                                  std::to_string(plugin_api_version));
    }

    GraphicsPlugin::Backend backend(create(), destroy);
    if (!backend) {
        return rejected(path, std::string(create_symbol) + " returned null");
    }

    return {.plugin = GraphicsPlugin(std::move(library), std::move(backend)), .error = ""};
}

} // namespace engine::graphics
