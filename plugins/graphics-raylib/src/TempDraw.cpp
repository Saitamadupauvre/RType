// TEMPORARY(#72): whole file. Placeholder drawing until the real renderer API lands.
// Called by the client between IRenderer::begin_frame and end_frame. Delete this file,
// its line in CMakeLists.txt and game/client/TempDraw.* together.

#include <raylib.h>

#include <cstdint>
#include <string>
#include <unordered_map>

#include "engine/graphics/PluginApi.hpp"

namespace {

std::unordered_map<std::string, Texture2D>& textures() {
    static std::unordered_map<std::string, Texture2D> cache;
    return cache;
}

const Texture2D* find_texture(const char* path) {
    auto& cache = textures();
    auto found = cache.find(path);
    if (found == cache.end()) {
        Texture2D texture = FileExists(path) ? LoadTexture(path) : Texture2D{};
        if (!IsTextureValid(texture)) {
            TraceLog(LOG_WARNING, "temp draw: cannot load %s", path);
        }
        found = cache.emplace(path, texture).first;
    }
    return IsTextureValid(found->second) ? &found->second : nullptr;
}

} // namespace

ENGINE_GRAPHICS_PLUGIN_EXPORT void engine_temp_draw_sprite(const char* path, float x, float y,
                                                           std::uint8_t r, std::uint8_t g,
                                                           std::uint8_t b, std::uint8_t a) {
    if (!IsWindowReady()) {
        return;
    }
    if (const Texture2D* texture = find_texture(path)) {
        DrawTextureV(*texture, {.x = x, .y = y}, {.r = r, .g = g, .b = b, .a = a});
    }
}

ENGINE_GRAPHICS_PLUGIN_EXPORT void engine_temp_draw_rect(float x, float y, float width,
                                                         float height, std::uint8_t r,
                                                         std::uint8_t g, std::uint8_t b,
                                                         std::uint8_t a) {
    if (!IsWindowReady()) {
        return;
    }
    DrawRectangleRec({.x = x, .y = y, .width = width, .height = height},
                     {.r = r, .g = g, .b = b, .a = a});
}
