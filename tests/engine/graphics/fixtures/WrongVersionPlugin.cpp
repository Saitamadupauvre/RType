#include <cstdint>

#include "FixtureBackend.hpp"
#include "engine/graphics/PluginApi.hpp"

ENGINE_GRAPHICS_PLUGIN_EXPORT std::uint32_t engine_graphics_api_version() {
    return engine::graphics::plugin_api_version + 1;
}

ENGINE_GRAPHICS_PLUGIN_EXPORT engine::graphics::IGraphicsBackend* engine_graphics_create() {
    return new fixture::Backend();
}

ENGINE_GRAPHICS_PLUGIN_EXPORT void
engine_graphics_destroy(engine::graphics::IGraphicsBackend* backend) {
    delete backend;
}
