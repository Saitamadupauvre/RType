#include "RaylibInput.hpp"

#include <raylib.h>

namespace {

using engine::graphics::Key;

KeyboardKey to_raylib(Key key) {
    switch (key) {
    case Key::A:
        return KEY_A;
    case Key::B:
        return KEY_B;
    case Key::C:
        return KEY_C;
    case Key::D:
        return KEY_D;
    case Key::E:
        return KEY_E;
    case Key::F:
        return KEY_F;
    case Key::G:
        return KEY_G;
    case Key::H:
        return KEY_H;
    case Key::I:
        return KEY_I;
    case Key::J:
        return KEY_J;
    case Key::K:
        return KEY_K;
    case Key::L:
        return KEY_L;
    case Key::M:
        return KEY_M;
    case Key::N:
        return KEY_N;
    case Key::O:
        return KEY_O;
    case Key::P:
        return KEY_P;
    case Key::Q:
        return KEY_Q;
    case Key::R:
        return KEY_R;
    case Key::S:
        return KEY_S;
    case Key::T:
        return KEY_T;
    case Key::U:
        return KEY_U;
    case Key::V:
        return KEY_V;
    case Key::W:
        return KEY_W;
    case Key::X:
        return KEY_X;
    case Key::Y:
        return KEY_Y;
    case Key::Z:
        return KEY_Z;
    case Key::Num0:
        return KEY_ZERO;
    case Key::Num1:
        return KEY_ONE;
    case Key::Num2:
        return KEY_TWO;
    case Key::Num3:
        return KEY_THREE;
    case Key::Num4:
        return KEY_FOUR;
    case Key::Num5:
        return KEY_FIVE;
    case Key::Num6:
        return KEY_SIX;
    case Key::Num7:
        return KEY_SEVEN;
    case Key::Num8:
        return KEY_EIGHT;
    case Key::Num9:
        return KEY_NINE;
    case Key::Up:
        return KEY_UP;
    case Key::Down:
        return KEY_DOWN;
    case Key::Left:
        return KEY_LEFT;
    case Key::Right:
        return KEY_RIGHT;
    case Key::Space:
        return KEY_SPACE;
    case Key::Enter:
        return KEY_ENTER;
    case Key::Escape:
        return KEY_ESCAPE;
    case Key::Tab:
        return KEY_TAB;
    case Key::Backspace:
        return KEY_BACKSPACE;
    case Key::LeftShift:
        return KEY_LEFT_SHIFT;
    case Key::RightShift:
        return KEY_RIGHT_SHIFT;
    case Key::LeftControl:
        return KEY_LEFT_CONTROL;
    case Key::RightControl:
        return KEY_RIGHT_CONTROL;
    case Key::LeftAlt:
        return KEY_LEFT_ALT;
    case Key::RightAlt:
        return KEY_RIGHT_ALT;
    case Key::Unknown:
        return KEY_NULL;
    }
    return KEY_NULL;
}

} // namespace

bool RaylibInput::is_key_down(Key key) const {
    const KeyboardKey code = to_raylib(key);
    return code != KEY_NULL && IsKeyDown(code);
}

bool RaylibInput::is_key_pressed(Key key) const {
    const KeyboardKey code = to_raylib(key);
    return code != KEY_NULL && IsKeyPressed(code);
}

bool RaylibInput::is_key_released(Key key) const {
    const KeyboardKey code = to_raylib(key);
    return code != KEY_NULL && IsKeyReleased(code);
}
