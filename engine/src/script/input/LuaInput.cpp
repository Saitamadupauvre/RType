#include "input/LuaInput.hpp"

#include <format>
#include <stdexcept>
#include <string>

#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

std::string describe_name(const sol::object& name) {
    if (name.get_type() == sol::type::string) {
        return std::format("'{}'", name.as<std::string_view>());
    }
    return std::format("of type {}", sol::type_name(name.lua_state(), name.get_type()));
}

bool client_pressed(const ClientInput& input, const sol::object& name, sol::this_state state) {
    const core::Key key = key_argument(name, state, "pressed");
    return input.current.keys.contains(key) && !input.previous.keys.contains(key);
}

} // namespace

core::Key key_argument(const sol::object& name, sol::this_state state, std::string_view method) {
    const auto key = name.get_type() == sol::type::string
                         ? core::key_from_name(name.as<std::string_view>())
                         : std::nullopt;
    if (!key) {
        throw std::runtime_error(std::format("{}input:{}: unknown key {}", caller_location(state),
                                             method, describe_name(name)));
    }
    return *key;
}

bool input_key(const core::InputState& input, const sol::object& name, sol::this_state state) {
    return input.keys.contains(key_argument(name, state, "key"));
}

bool input_mouse(const core::InputState& input, const sol::object& name, sol::this_state state) {
    const auto button = name.get_type() == sol::type::string
                            ? core::mouse_button_from_name(name.as<std::string_view>())
                            : std::nullopt;
    if (!button) {
        throw std::runtime_error(std::format("{}input:mouse: unknown mouse button {}",
                                             caller_location(state), describe_name(name)));
    }
    return input.mouse_buttons.contains(*button);
}

sol::variadic_results input_pointer(const core::InputState& input, sol::this_state state) {
    sol::variadic_results results;
    if (!input.pointer) {
        results.push_back(sol::make_object(state, sol::lua_nil));
        return results;
    }
    results.push_back(sol::make_object(state, input.pointer->x));
    results.push_back(sol::make_object(state, input.pointer->y));
    return results;
}

void bind_input_state(sol::table types) {
    types.new_usertype<core::InputState>("InputState", sol::no_constructor, "key", &input_key,
                                         "mouse", &input_mouse, "pointer", &input_pointer);
}

void bind_client_input(sol::table types) {
    types.new_usertype<ClientInput>(
        "ClientInput", sol::no_constructor, "key",
        [](const ClientInput& input, const sol::object& name, sol::this_state state) {
            return input_key(input.current, name, state);
        },
        "mouse",
        [](const ClientInput& input, const sol::object& name, sol::this_state state) {
            return input_mouse(input.current, name, state);
        },
        "pointer",
        [](const ClientInput& input, sol::this_state state) {
            return input_pointer(input.current, state);
        },
        "pressed", &client_pressed);
}

} // namespace engine::script
