#pragma once

#include <string>

#include "lua/Sol.hpp"

namespace engine::script {

std::string caller_location(lua_State* state);

bool report(const sol::protected_function_result& result);

} // namespace engine::script
