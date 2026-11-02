#pragma once

#include <cstdint>
#include <string>

#include "engine/core/Keys.hpp"

namespace engine::graphics {

/** @brief Axis-aligned rectangle in virtual units; origin at the top-left, Y pointing down. */
struct Rect {
    float x{0.F};
    float y{0.F};
    float width{0.F};
    float height{0.F};

    bool operator==(const Rect&) const = default;
};

/** @brief 8-bit RGBA color. */
struct Color {
    std::uint8_t r{0};
    std::uint8_t g{0};
    std::uint8_t b{0};
    std::uint8_t a{255};

    bool operator==(const Color&) const = default;
};

/**
 * @brief Handle to a texture owned by a renderer.
 *
 * @note The default value refers to no texture.
 */
struct TextureId {
    std::uint32_t value{0};

    bool operator==(const TextureId&) const = default;
};

/** @brief Part of a texture drawn into a rectangle of the virtual space. */
struct SpriteDraw {
    /** @brief Texture to draw; the default value draws the placeholder texture. */
    TextureId texture;
    /** @brief Region of the texture to draw, in texture pixels. */
    Rect source;
    /** @brief Where the region is drawn, in virtual units; it is stretched to fit. */
    Rect dest;
    /** @brief Color multiplied with the texture; white keeps it unchanged. */
    Color tint{.r = 255, .g = 255, .b = 255, .a = 255};
};

/** @brief Settings used to open a window. */
struct WindowConfig {
    int width{1280};
    int height{720};
    std::string title{"engine"};
    bool fullscreen{false};
};

/** @brief Mouse buttons, independent of any backend. */
using MouseButton = core::MouseButton;

/** @brief Physical keyboard keys, independent of any backend. */
using Key = core::Key;

} // namespace engine::graphics
