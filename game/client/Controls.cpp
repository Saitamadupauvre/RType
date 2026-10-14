#include "Controls.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace rtype::client {

namespace {

using engine::graphics::Key;

struct Binding {
    std::string_view action;
    std::array<Key, 2> keys;
};

constexpr std::array bindings{
    Binding{.action = "move_up", .keys = {Key::Up, Key::Z}},
    Binding{.action = "move_down", .keys = {Key::Down, Key::S}},
    Binding{.action = "move_left", .keys = {Key::Left, Key::Q}},
    Binding{.action = "move_right", .keys = {Key::Right, Key::D}},
    Binding{.action = "fire", .keys = {Key::Space, Key::Unknown}},
};

bool action_down(const engine::graphics::IInput& input, std::string_view action) {
    const auto binding = std::ranges::find(bindings, action, &Binding::action);
    return std::ranges::any_of(binding->keys, [&](Key key) { return input.is_key_down(key); });
}

float axis(const engine::graphics::IInput& input, std::string_view negative,
           std::string_view positive) {
    return (action_down(input, positive) ? 1.F : 0.F) - (action_down(input, negative) ? 1.F : 0.F);
}

} // namespace

engine::script::InputState read_controls(const engine::graphics::IInput& input) {
    engine::script::InputState state;
    state.axes.emplace("move_x", axis(input, "move_left", "move_right"));
    state.axes.emplace("move_y", axis(input, "move_up", "move_down"));
    for (const Binding& binding : bindings) {
        if (action_down(input, binding.action)) {
            state.pressed.emplace(binding.action);
        }
    }
    return state;
}

} // namespace rtype::client
