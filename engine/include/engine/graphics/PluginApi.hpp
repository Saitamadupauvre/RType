#pragma once

#include <cstdint>

namespace engine::graphics {

/**
 * @brief Version of the graphics plugin interface.
 *
 * Incremented on every change to the plugin interface. The plugin loader
 * rejects any plugin whose `engine_graphics_api_version()` returns a
 * different value.
 */
inline constexpr std::uint32_t plugin_api_version = 1;

} // namespace engine::graphics
