#pragma once

#include <string>

#include "lua/Sol.hpp"

namespace engine::script {

std::string caller_location(lua_State* state);

std::string explain_lua_error(std::string message);

bool report(const sol::protected_function_result& result);

} // namespace engine::script
