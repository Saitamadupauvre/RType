#pragma once

#include "engine/core/InputState.hpp"
#include "engine/core/Keys.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

struct ClientInput {
    core::InputState current;
    core::InputState previous;
};

core::Key key_argument(const sol::object& name, sol::this_state state, std::string_view method);
bool input_key(const core::InputState& input, const sol::object& name, sol::this_state state);
bool input_mouse(const core::InputState& input, const sol::object& name, sol::this_state state);
sol::variadic_results input_pointer(const core::InputState& input, sol::this_state state);

void bind_input_state(sol::table types);
void bind_client_input(sol::table types);

} // namespace engine::script
