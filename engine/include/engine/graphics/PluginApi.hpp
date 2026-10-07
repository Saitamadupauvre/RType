#pragma once

#include <cstdint>
#include <string_view>

#include "engine/graphics/IGraphicsBackend.hpp"

namespace engine::graphics {

/**
 * @brief Version of the graphics plugin interface.
 *
 * Incremented on every change to the plugin interface. The plugin loader
 * rejects any plugin whose `engine_graphics_api_version()` returns a
 * different value.
 */
inline constexpr std::uint32_t plugin_api_version = 2;

/** @brief `extern "C" std::uint32_t engine_graphics_api_version()`: returns plugin_api_version. */
using ApiVersionFunction = std::uint32_t (*)();

/** @brief `extern "C" IGraphicsBackend* engine_graphics_create()`: allocates the backend. */
using CreateFunction = IGraphicsBackend* (*)();

/** @brief `extern "C" void engine_graphics_destroy(IGraphicsBackend*)`: frees the backend. */
using DestroyFunction = void (*)(IGraphicsBackend*);

/** @brief Exported symbol names a graphics plugin must provide. */
inline constexpr std::string_view api_version_symbol = "engine_graphics_api_version";
inline constexpr std::string_view create_symbol = "engine_graphics_create";
inline constexpr std::string_view destroy_symbol = "engine_graphics_destroy";

} // namespace engine::graphics
