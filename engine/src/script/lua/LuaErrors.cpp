#include "LuaErrors.hpp"

#include "engine/core/log/Log.hpp"

namespace engine::script {

std::string caller_location(lua_State* state) {
    luaL_where(state, 1);
    std::string location = lua_tostring(state, -1);
    lua_pop(state, 1);
    return location;
}

bool report(const sol::protected_function_result& result) {
    if (result.valid()) {
        return true;
    }
    const sol::error error = result;
    engine::core::log::error("lua: {}", error.what());
    return false;
}

} // namespace engine::script
