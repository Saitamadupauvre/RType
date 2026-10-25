#include <cstdint>

#include "engine/graphics/PluginApi.hpp"

ENGINE_GRAPHICS_PLUGIN_EXPORT std::uint32_t engine_graphics_api_version() {
    return engine::graphics::plugin_api_version;
}

ENGINE_GRAPHICS_PLUGIN_EXPORT engine::graphics::IGraphicsBackend* engine_graphics_create() {
    return nullptr;
}

ENGINE_GRAPHICS_PLUGIN_EXPORT void
engine_graphics_destroy(engine::graphics::IGraphicsBackend* backend) {
    delete backend;
}
