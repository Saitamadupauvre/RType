#include "engine/graphics/InputCapture.hpp"

namespace engine::graphics {

core::InputState capture_input(const IInput& input) {
    core::InputState state;
    for (const core::KeyInfo& info : core::keys) {
        if (input.is_key_down(info.key)) {
            state.keys.insert(info.key);
        }
    }
    for (const core::MouseButtonInfo& info : core::mouse_buttons) {
        if (input.is_mouse_down(info.button)) {
            state.mouse_buttons.insert(info.button);
        }
    }
    state.pointer = input.mouse_position();
    return state;
}

} // namespace engine::graphics
