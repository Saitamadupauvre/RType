#include <cstdint>

#include "FixtureBackend.hpp"
#include "engine/graphics/PluginApi.hpp"

namespace {

int destroyed_count = 0;

} // namespace

ENGINE_GRAPHICS_PLUGIN_EXPORT std::uint32_t engine_graphics_api_version() {
    return engine::graphics::plugin_api_version;
}

ENGINE_GRAPHICS_PLUGIN_EXPORT engine::graphics::IGraphicsBackend* engine_graphics_create() {
    return new fixture::Backend();
}

ENGINE_GRAPHICS_PLUGIN_EXPORT void
engine_graphics_destroy(engine::graphics::IGraphicsBackend* backend) {
    delete backend;
    ++destroyed_count;
}

ENGINE_GRAPHICS_PLUGIN_EXPORT int fixture_destroyed_count() { return destroyed_count; }
