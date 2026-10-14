// TEMPORARY(#72): whole file. Placeholder drawing until the real renderer API lands.
// Delete with game/client/TempDraw.cpp and plugins/graphics-raylib/src/TempDraw.cpp.

#pragma once

#include <cstdint>
#include <filesystem>

#include "engine/core/DynamicLibrary.hpp"
#include "engine/core/EntityManager.hpp"

namespace rtype::client {

class TempDraw {
public:
    TempDraw(const std::filesystem::path& plugin, std::filesystem::path assets);

    [[nodiscard]] bool ready() const;
    void draw_world(engine::core::EntityManager& world) const;

private:
    using DrawSprite = void (*)(const char*, float, float, std::uint8_t, std::uint8_t, std::uint8_t,
                                std::uint8_t);
    using DrawRect = void (*)(float, float, float, float, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t);

    engine::core::DynamicLibrary _library;
    std::filesystem::path _assets;
    DrawSprite _draw_sprite = nullptr;
    DrawRect _draw_rect = nullptr;
};

} // namespace rtype::client
