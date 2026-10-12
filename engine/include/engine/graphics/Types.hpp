#pragma once

#include <cstdint>
#include <string>

namespace engine::graphics {

/** @brief 2D vector in virtual units. */
struct Vec2 {
    float x{0.F};
    float y{0.F};

    bool operator==(const Vec2&) const = default;
};

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

/** @brief Settings used to open a window. */
struct WindowConfig {
    int width{1280};
    int height{720};
    std::string title{"engine"};
    bool fullscreen{false};
};

/** @brief Physical keyboard keys, independent of any backend. */
enum class Key : std::uint8_t {
    Unknown,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    Num0,
    Num1,
    Num2,
    Num3,
    Num4,
    Num5,
    Num6,
    Num7,
    Num8,
    Num9,
    Up,
    Down,
    Left,
    Right,
    Space,
    Enter,
    Escape,
    Tab,
    Backspace,
    LeftShift,
    RightShift,
    LeftControl,
    RightControl,
    LeftAlt,
    RightAlt,
};

} // namespace engine::graphics
