#include "RaylibInput.hpp"

#include <raylib.h>

#include "KeyMap.hpp"
#include "Letterbox.hpp"
#include "engine/graphics/Types.hpp"

using engine::core::Vec2;
using engine::graphics::Key;
using Button = engine::graphics::MouseButton;

namespace {

int to_raylib(Button button) {
    switch (button) {
    case Button::Left:
        return MOUSE_BUTTON_LEFT;
    case Button::Right:
        return MOUSE_BUTTON_RIGHT;
    case Button::Middle:
        return MOUSE_BUTTON_MIDDLE;
    }
    return MOUSE_BUTTON_LEFT;
}

} // namespace

bool RaylibInput::is_key_down(Key key) const {
    const int code = to_raylib(key);
    return code != KEY_NULL && IsKeyDown(code);
}

bool RaylibInput::is_key_pressed(Key key) const {
    const int code = to_raylib(key);
    return code != KEY_NULL && IsKeyPressed(code);
}

bool RaylibInput::is_key_released(Key key) const {
    const int code = to_raylib(key);
    return code != KEY_NULL && IsKeyReleased(code);
}

Key RaylibInput::last_key_pressed() {
    for (int code = GetKeyPressed(); code != KEY_NULL; code = GetKeyPressed()) {
        const Key key = from_raylib(code);
        if (key != Key::Unknown) {
            return key;
        }
    }
    return Key::Unknown;
}

Vec2 RaylibInput::mouse_position() const {
    const ::Vector2 mouse = GetMousePosition();
    const Vec2 window{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())};
    return window_to_virtual({mouse.x, mouse.y}, window, virtual_resolution);
}

bool RaylibInput::is_mouse_down(Button button) const {
    return IsMouseButtonDown(to_raylib(button));
}

bool RaylibInput::is_mouse_pressed(Button button) const {
    return IsMouseButtonPressed(to_raylib(button));
}

bool RaylibInput::is_mouse_released(Button button) const {
    return IsMouseButtonReleased(to_raylib(button));
}