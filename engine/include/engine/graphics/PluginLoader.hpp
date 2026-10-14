#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "engine/core/DynamicLibrary.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/IGraphicsBackend.hpp"
#include "engine/graphics/PluginApi.hpp"

namespace engine::graphics {

struct PluginLoadResult;

/**
 * @brief A loaded graphics plugin and the backend it created.
 *
 * The backend is destroyed by the plugin's own engine_graphics_destroy(), before the library is
 * closed.
 *
 * @note Move-constructible only: move assignment is deleted because it would close the old library
 * before destroying the backend it created. Main thread only.
 */
class GraphicsPlugin {
public:
    GraphicsPlugin(const GraphicsPlugin&) = delete;
    GraphicsPlugin& operator=(const GraphicsPlugin&) = delete;
    GraphicsPlugin(GraphicsPlugin&&) noexcept = default;
    GraphicsPlugin& operator=(GraphicsPlugin&&) = delete;
    ~GraphicsPlugin() = default;

    /** @return The backend created by the plugin. */
    [[nodiscard]] IGraphicsBackend& backend() noexcept { return *_backend; }

private:
    using Backend = std::unique_ptr<IGraphicsBackend, DestroyFunction>;

    GraphicsPlugin(core::DynamicLibrary library, Backend backend)
        : _library(std::move(library)), _backend(std::move(backend)) {}

    friend ENGINE_GRAPHICS_EXPORT PluginLoadResult
    load_graphics_plugin(const std::filesystem::path&);

    core::DynamicLibrary _library;
    Backend _backend;
};

/** @brief Outcome of load_graphics_plugin(): a plugin, or the reason it was rejected. */
struct PluginLoadResult {
    std::optional<GraphicsPlugin> plugin;
    std::string error;
};

/**
 * @brief Opens a graphics plugin, checks its API version and creates its backend.
 *
 * @param path Plugin file, used as is; resolving it relative to the executable is the caller's job.
 * @return The plugin, or an empty optional and an error naming the path when the file cannot be
 * opened, an entry point is missing, the API version differs from plugin_api_version, or
 * engine_graphics_create() returns null.
 * @note Never throws on a bad plugin.
 */
[[nodiscard]] ENGINE_GRAPHICS_EXPORT PluginLoadResult
load_graphics_plugin(const std::filesystem::path& path);

} // namespace engine::graphics
