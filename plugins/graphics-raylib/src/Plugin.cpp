#include <cstdint>

#include "engine/graphics/PluginApi.hpp"

#if defined(_WIN32)
#define PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT std::uint32_t engine_graphics_api_version() {
    return engine::graphics::plugin_api_version;
}
