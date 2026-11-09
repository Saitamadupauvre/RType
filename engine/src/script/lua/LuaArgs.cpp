#include "lua/LuaArgs.hpp"

#include <cmath>

namespace engine::script {

bool is_nil(const sol::object& value) {
    return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
}

std::optional<float> finite_number(const sol::object& value) {
    if (value.get_type() != sol::type::number || !std::isfinite(value.as<double>())) {
        return std::nullopt;
    }
    return value.as<float>();
}

std::optional<float> positive_number(const sol::object& value) {
    const auto number = finite_number(value);
    if (!number || value.as<double>() <= 0.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<lua_Integer> non_negative_integer(sol::this_state state, const sol::object& value) {
    if (value.get_type() != sol::type::number) {
        return std::nullopt;
    }
    lua_State* lua = state;
    value.push(lua);
    const bool integer = lua_isinteger(lua, -1) != 0;
    lua_pop(lua, 1);
    if (!integer || value.as<lua_Integer>() < 0) {
        return std::nullopt;
    }
    return value.as<lua_Integer>();
}

} // namespace engine::script
