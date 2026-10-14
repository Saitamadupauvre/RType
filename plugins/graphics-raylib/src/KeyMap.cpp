#include "KeyMap.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include <raylib.h>

namespace {

using engine::graphics::Key;

constexpr std::array key_map{
    std::pair{Key::A, KEY_A},
    std::pair{Key::B, KEY_B},
    std::pair{Key::C, KEY_C},
    std::pair{Key::D, KEY_D},
    std::pair{Key::E, KEY_E},
    std::pair{Key::F, KEY_F},
    std::pair{Key::G, KEY_G},
    std::pair{Key::H, KEY_H},
    std::pair{Key::I, KEY_I},
    std::pair{Key::J, KEY_J},
    std::pair{Key::K, KEY_K},
    std::pair{Key::L, KEY_L},
    std::pair{Key::M, KEY_M},
    std::pair{Key::N, KEY_N},
    std::pair{Key::O, KEY_O},
    std::pair{Key::P, KEY_P},
    std::pair{Key::Q, KEY_Q},
    std::pair{Key::R, KEY_R},
    std::pair{Key::S, KEY_S},
    std::pair{Key::T, KEY_T},
    std::pair{Key::U, KEY_U},
    std::pair{Key::V, KEY_V},
    std::pair{Key::W, KEY_W},
    std::pair{Key::X, KEY_X},
    std::pair{Key::Y, KEY_Y},
    std::pair{Key::Z, KEY_Z},
    std::pair{Key::Num0, KEY_ZERO},
    std::pair{Key::Num1, KEY_ONE},
    std::pair{Key::Num2, KEY_TWO},
    std::pair{Key::Num3, KEY_THREE},
    std::pair{Key::Num4, KEY_FOUR},
    std::pair{Key::Num5, KEY_FIVE},
    std::pair{Key::Num6, KEY_SIX},
    std::pair{Key::Num7, KEY_SEVEN},
    std::pair{Key::Num8, KEY_EIGHT},
    std::pair{Key::Num9, KEY_NINE},
    std::pair{Key::Numpad0, KEY_KP_0},
    std::pair{Key::Numpad1, KEY_KP_1},
    std::pair{Key::Numpad2, KEY_KP_2},
    std::pair{Key::Numpad3, KEY_KP_3},
    std::pair{Key::Numpad4, KEY_KP_4},
    std::pair{Key::Numpad5, KEY_KP_5},
    std::pair{Key::Numpad6, KEY_KP_6},
    std::pair{Key::Numpad7, KEY_KP_7},
    std::pair{Key::Numpad8, KEY_KP_8},
    std::pair{Key::Numpad9, KEY_KP_9},
    std::pair{Key::NumpadDecimal, KEY_KP_DECIMAL},
    std::pair{Key::NumpadDivide, KEY_KP_DIVIDE},
    std::pair{Key::NumpadMultiply, KEY_KP_MULTIPLY},
    std::pair{Key::NumpadSubtract, KEY_KP_SUBTRACT},
    std::pair{Key::NumpadAdd, KEY_KP_ADD},
    std::pair{Key::NumpadEnter, KEY_KP_ENTER},
    std::pair{Key::NumpadEqual, KEY_KP_EQUAL},
    std::pair{Key::Up, KEY_UP},
    std::pair{Key::Down, KEY_DOWN},
    std::pair{Key::Left, KEY_LEFT},
    std::pair{Key::Right, KEY_RIGHT},
    std::pair{Key::Space, KEY_SPACE},
    std::pair{Key::Enter, KEY_ENTER},
    std::pair{Key::Escape, KEY_ESCAPE},
    std::pair{Key::Tab, KEY_TAB},
    std::pair{Key::Backspace, KEY_BACKSPACE},
    std::pair{Key::LeftShift, KEY_LEFT_SHIFT},
    std::pair{Key::RightShift, KEY_RIGHT_SHIFT},
    std::pair{Key::LeftControl, KEY_LEFT_CONTROL},
    std::pair{Key::RightControl, KEY_RIGHT_CONTROL},
    std::pair{Key::LeftAlt, KEY_LEFT_ALT},
    std::pair{Key::RightAlt, KEY_RIGHT_ALT},
    std::pair{Key::LeftSuper, KEY_LEFT_SUPER},
    std::pair{Key::RightSuper, KEY_RIGHT_SUPER},
    std::pair{Key::Menu, KEY_KB_MENU},
    std::pair{Key::Insert, KEY_INSERT},
    std::pair{Key::Delete, KEY_DELETE},
    std::pair{Key::Home, KEY_HOME},
    std::pair{Key::End, KEY_END},
    std::pair{Key::PageUp, KEY_PAGE_UP},
    std::pair{Key::PageDown, KEY_PAGE_DOWN},
    std::pair{Key::CapsLock, KEY_CAPS_LOCK},
    std::pair{Key::ScrollLock, KEY_SCROLL_LOCK},
    std::pair{Key::NumLock, KEY_NUM_LOCK},
    std::pair{Key::PrintScreen, KEY_PRINT_SCREEN},
    std::pair{Key::Pause, KEY_PAUSE},
    std::pair{Key::F1, KEY_F1},
    std::pair{Key::F2, KEY_F2},
    std::pair{Key::F3, KEY_F3},
    std::pair{Key::F4, KEY_F4},
    std::pair{Key::F5, KEY_F5},
    std::pair{Key::F6, KEY_F6},
    std::pair{Key::F7, KEY_F7},
    std::pair{Key::F8, KEY_F8},
    std::pair{Key::F9, KEY_F9},
    std::pair{Key::F10, KEY_F10},
    std::pair{Key::F11, KEY_F11},
    std::pair{Key::F12, KEY_F12},
    std::pair{Key::Apostrophe, KEY_APOSTROPHE},
    std::pair{Key::Comma, KEY_COMMA},
    std::pair{Key::Minus, KEY_MINUS},
    std::pair{Key::Period, KEY_PERIOD},
    std::pair{Key::Slash, KEY_SLASH},
    std::pair{Key::Semicolon, KEY_SEMICOLON},
    std::pair{Key::Equal, KEY_EQUAL},
    std::pair{Key::LeftBracket, KEY_LEFT_BRACKET},
    std::pair{Key::RightBracket, KEY_RIGHT_BRACKET},
    std::pair{Key::Backslash, KEY_BACKSLASH},
    std::pair{Key::GraveAccent, KEY_GRAVE},
};

} // namespace

int to_raylib(Key key) {
    const auto* entry = std::ranges::find(key_map, key, &std::pair<Key, KeyboardKey>::first);
    return entry == key_map.end() ? KEY_NULL : entry->second;
}

Key from_raylib(int code) {
    const auto* entry = std::ranges::find(key_map, code, &std::pair<Key, KeyboardKey>::second);
    return entry == key_map.end() ? Key::Unknown : entry->first;
}
