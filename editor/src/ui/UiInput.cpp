#include "ui/UiInput.hpp"

#include <algorithm>

namespace editor {

bool pressed_or_repeated(const UiInput& input, Key key) {
    return std::ranges::find(input.keys_pressed, key) != input.keys_pressed.end() ||
           std::ranges::find(input.keys_repeated, key) != input.keys_repeated.end();
}

} // namespace editor
