#pragma once

#include <cstdint>
#include <string>

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

/** @brief Settings used to open a window. */
struct WindowConfig {
    int width{1280};
    int height{720};
    std::string title{"engine"};
    bool fullscreen{false};
};

/** @brief Mouse buttons, independent of any backend. */
enum class MouseButton : std::uint8_t {
    Left,
    Right,
    Middle,
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
    Numpad0,
    Numpad1,
    Numpad2,
    Numpad3,
    Numpad4,
    Numpad5,
    Numpad6,
    Numpad7,
    Numpad8,
    Numpad9,
    NumpadDecimal,
    NumpadDivide,
    NumpadMultiply,
    NumpadSubtract,
    NumpadAdd,
    NumpadEnter,
    NumpadEqual,
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
    LeftSuper,
    RightSuper,
    Menu,
    Insert,
    Delete,
    Home,
    End,
    PageUp,
    PageDown,
    CapsLock,
    ScrollLock,
    NumLock,
    PrintScreen,
    Pause,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Apostrophe,
    Comma,
    Minus,
    Period,
    Slash,
    Semicolon,
    Equal,
    LeftBracket,
    RightBracket,
    Backslash,
    GraveAccent,
};

} // namespace engine::graphics
