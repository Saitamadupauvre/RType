#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace engine::core {

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

/** @brief A key and the name scripts use for it, spelled like its enumerator. */
struct KeyInfo {
    Key key;
    std::string_view name;
};

/** @brief Every key but Key::Unknown, in enumeration order. */
inline constexpr std::array keys{
    KeyInfo{.key = Key::A, .name = "A"},
    KeyInfo{.key = Key::B, .name = "B"},
    KeyInfo{.key = Key::C, .name = "C"},
    KeyInfo{.key = Key::D, .name = "D"},
    KeyInfo{.key = Key::E, .name = "E"},
    KeyInfo{.key = Key::F, .name = "F"},
    KeyInfo{.key = Key::G, .name = "G"},
    KeyInfo{.key = Key::H, .name = "H"},
    KeyInfo{.key = Key::I, .name = "I"},
    KeyInfo{.key = Key::J, .name = "J"},
    KeyInfo{.key = Key::K, .name = "K"},
    KeyInfo{.key = Key::L, .name = "L"},
    KeyInfo{.key = Key::M, .name = "M"},
    KeyInfo{.key = Key::N, .name = "N"},
    KeyInfo{.key = Key::O, .name = "O"},
    KeyInfo{.key = Key::P, .name = "P"},
    KeyInfo{.key = Key::Q, .name = "Q"},
    KeyInfo{.key = Key::R, .name = "R"},
    KeyInfo{.key = Key::S, .name = "S"},
    KeyInfo{.key = Key::T, .name = "T"},
    KeyInfo{.key = Key::U, .name = "U"},
    KeyInfo{.key = Key::V, .name = "V"},
    KeyInfo{.key = Key::W, .name = "W"},
    KeyInfo{.key = Key::X, .name = "X"},
    KeyInfo{.key = Key::Y, .name = "Y"},
    KeyInfo{.key = Key::Z, .name = "Z"},
    KeyInfo{.key = Key::Num0, .name = "Num0"},
    KeyInfo{.key = Key::Num1, .name = "Num1"},
    KeyInfo{.key = Key::Num2, .name = "Num2"},
    KeyInfo{.key = Key::Num3, .name = "Num3"},
    KeyInfo{.key = Key::Num4, .name = "Num4"},
    KeyInfo{.key = Key::Num5, .name = "Num5"},
    KeyInfo{.key = Key::Num6, .name = "Num6"},
    KeyInfo{.key = Key::Num7, .name = "Num7"},
    KeyInfo{.key = Key::Num8, .name = "Num8"},
    KeyInfo{.key = Key::Num9, .name = "Num9"},
    KeyInfo{.key = Key::Numpad0, .name = "Numpad0"},
    KeyInfo{.key = Key::Numpad1, .name = "Numpad1"},
    KeyInfo{.key = Key::Numpad2, .name = "Numpad2"},
    KeyInfo{.key = Key::Numpad3, .name = "Numpad3"},
    KeyInfo{.key = Key::Numpad4, .name = "Numpad4"},
    KeyInfo{.key = Key::Numpad5, .name = "Numpad5"},
    KeyInfo{.key = Key::Numpad6, .name = "Numpad6"},
    KeyInfo{.key = Key::Numpad7, .name = "Numpad7"},
    KeyInfo{.key = Key::Numpad8, .name = "Numpad8"},
    KeyInfo{.key = Key::Numpad9, .name = "Numpad9"},
    KeyInfo{.key = Key::NumpadDecimal, .name = "NumpadDecimal"},
    KeyInfo{.key = Key::NumpadDivide, .name = "NumpadDivide"},
    KeyInfo{.key = Key::NumpadMultiply, .name = "NumpadMultiply"},
    KeyInfo{.key = Key::NumpadSubtract, .name = "NumpadSubtract"},
    KeyInfo{.key = Key::NumpadAdd, .name = "NumpadAdd"},
    KeyInfo{.key = Key::NumpadEnter, .name = "NumpadEnter"},
    KeyInfo{.key = Key::NumpadEqual, .name = "NumpadEqual"},
    KeyInfo{.key = Key::Up, .name = "Up"},
    KeyInfo{.key = Key::Down, .name = "Down"},
    KeyInfo{.key = Key::Left, .name = "Left"},
    KeyInfo{.key = Key::Right, .name = "Right"},
    KeyInfo{.key = Key::Space, .name = "Space"},
    KeyInfo{.key = Key::Enter, .name = "Enter"},
    KeyInfo{.key = Key::Escape, .name = "Escape"},
    KeyInfo{.key = Key::Tab, .name = "Tab"},
    KeyInfo{.key = Key::Backspace, .name = "Backspace"},
    KeyInfo{.key = Key::LeftShift, .name = "LeftShift"},
    KeyInfo{.key = Key::RightShift, .name = "RightShift"},
    KeyInfo{.key = Key::LeftControl, .name = "LeftControl"},
    KeyInfo{.key = Key::RightControl, .name = "RightControl"},
    KeyInfo{.key = Key::LeftAlt, .name = "LeftAlt"},
    KeyInfo{.key = Key::RightAlt, .name = "RightAlt"},
    KeyInfo{.key = Key::LeftSuper, .name = "LeftSuper"},
    KeyInfo{.key = Key::RightSuper, .name = "RightSuper"},
    KeyInfo{.key = Key::Menu, .name = "Menu"},
    KeyInfo{.key = Key::Insert, .name = "Insert"},
    KeyInfo{.key = Key::Delete, .name = "Delete"},
    KeyInfo{.key = Key::Home, .name = "Home"},
    KeyInfo{.key = Key::End, .name = "End"},
    KeyInfo{.key = Key::PageUp, .name = "PageUp"},
    KeyInfo{.key = Key::PageDown, .name = "PageDown"},
    KeyInfo{.key = Key::CapsLock, .name = "CapsLock"},
    KeyInfo{.key = Key::ScrollLock, .name = "ScrollLock"},
    KeyInfo{.key = Key::NumLock, .name = "NumLock"},
    KeyInfo{.key = Key::PrintScreen, .name = "PrintScreen"},
    KeyInfo{.key = Key::Pause, .name = "Pause"},
    KeyInfo{.key = Key::F1, .name = "F1"},
    KeyInfo{.key = Key::F2, .name = "F2"},
    KeyInfo{.key = Key::F3, .name = "F3"},
    KeyInfo{.key = Key::F4, .name = "F4"},
    KeyInfo{.key = Key::F5, .name = "F5"},
    KeyInfo{.key = Key::F6, .name = "F6"},
    KeyInfo{.key = Key::F7, .name = "F7"},
    KeyInfo{.key = Key::F8, .name = "F8"},
    KeyInfo{.key = Key::F9, .name = "F9"},
    KeyInfo{.key = Key::F10, .name = "F10"},
    KeyInfo{.key = Key::F11, .name = "F11"},
    KeyInfo{.key = Key::F12, .name = "F12"},
    KeyInfo{.key = Key::Apostrophe, .name = "Apostrophe"},
    KeyInfo{.key = Key::Comma, .name = "Comma"},
    KeyInfo{.key = Key::Minus, .name = "Minus"},
    KeyInfo{.key = Key::Period, .name = "Period"},
    KeyInfo{.key = Key::Slash, .name = "Slash"},
    KeyInfo{.key = Key::Semicolon, .name = "Semicolon"},
    KeyInfo{.key = Key::Equal, .name = "Equal"},
    KeyInfo{.key = Key::LeftBracket, .name = "LeftBracket"},
    KeyInfo{.key = Key::RightBracket, .name = "RightBracket"},
    KeyInfo{.key = Key::Backslash, .name = "Backslash"},
    KeyInfo{.key = Key::GraveAccent, .name = "GraveAccent"},
};

/** @brief A mouse button and the name scripts use for it. */
struct MouseButtonInfo {
    MouseButton button;
    std::string_view name;
};

/** @brief Every mouse button, in enumeration order. */
inline constexpr std::array mouse_buttons{
    MouseButtonInfo{.button = MouseButton::Left, .name = "Left"},
    MouseButtonInfo{.button = MouseButton::Right, .name = "Right"},
    MouseButtonInfo{.button = MouseButton::Middle, .name = "Middle"},
};

namespace detail {

consteval bool keys_follow_enumeration() {
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (static_cast<std::size_t>(keys.at(index).key) != index + 1) {
            return false;
        }
    }
    return true;
}

consteval bool mouse_buttons_follow_enumeration() {
    for (std::size_t index = 0; index < mouse_buttons.size(); ++index) {
        if (static_cast<std::size_t>(mouse_buttons.at(index).button) != index) {
            return false;
        }
    }
    return true;
}

} // namespace detail

static_assert(detail::keys_follow_enumeration(), "keys must list every Key in order");
static_assert(detail::mouse_buttons_follow_enumeration(),
              "mouse_buttons must list every MouseButton in order");

/** @brief Number of Key values, Key::Unknown included. */
inline constexpr std::size_t key_count = keys.size() + 1;

/** @brief Number of MouseButton values. */
inline constexpr std::size_t mouse_button_count = mouse_buttons.size();

/**
 * @return The key named @p name, case-sensitive, or `std::nullopt` when no key has this name.
 * `"Unknown"` is not a key name.
 */
[[nodiscard]] constexpr std::optional<Key> key_from_name(std::string_view name) noexcept {
    const auto found = std::ranges::find(keys, name, &KeyInfo::name);
    return found == keys.end() ? std::nullopt : std::optional<Key>{found->key};
}

/**
 * @return The mouse button named @p name, case-sensitive, or `std::nullopt` when absent.
 */
[[nodiscard]] constexpr std::optional<MouseButton>
mouse_button_from_name(std::string_view name) noexcept {
    const auto found = std::ranges::find(mouse_buttons, name, &MouseButtonInfo::name);
    return found == mouse_buttons.end() ? std::nullopt : std::optional<MouseButton>{found->button};
}

} // namespace engine::core
