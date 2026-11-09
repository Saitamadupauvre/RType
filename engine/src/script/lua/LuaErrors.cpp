#include "LuaErrors.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string_view>

#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

constexpr std::array<std::string_view, 14> planned_names{
    "find_all", "find_first", "count",  "after",   "every",      "wait",        "random",
    "world",    "log",        "synced", "add_tag", "remove_tag", "distance_to", "move_to",
};

constexpr std::array<std::string_view, 2> nil_uses{"attempt to call a nil value",
                                                   "attempt to index a nil value"};

std::optional<std::string_view> missing_planned_name(std::string_view message) {
    for (const std::string_view use : nil_uses) {
        for (const std::string_view name : planned_names) {
            for (const std::string_view scope : {"global", "method", "field"}) {
                if (message.find(std::format("{} ({} '{}')", use, scope, name)) !=
                    std::string_view::npos) {
                    return name;
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace

std::string explain_lua_error(std::string message) {
    if (const auto name = missing_planned_name(message)) {
        const std::size_t line_end = std::min(message.find('\n'), message.size());
        message.insert(line_end, std::format(" ('{}' is planned but not implemented yet, see "
                                             "docs/reference/lua-api.md)",
                                             *name));
    }
    return message;
}

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
    engine::core::log::error("lua: {}", explain_lua_error(error.what()));
    return false;
}

} // namespace engine::script
