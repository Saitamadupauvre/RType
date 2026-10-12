#include "engine/script/ScriptRuntime.hpp"

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include <exception>
#include <string>

#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

bool report(const sol::protected_function_result& result) {
    if (result.valid()) {
        return true;
    }
    const sol::error error = result;
    log::error("lua: {}", error.what());
    return false;
}

} // namespace

struct ScriptRuntime::Impl {
    sol::state lua;
};

ScriptRuntime::ScriptRuntime() : _impl(std::make_unique<Impl>()) {
    _impl->lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                              sol::lib::coroutine);
    _impl->lua["dofile"] = sol::lua_nil;
    _impl->lua["loadfile"] = sol::lua_nil;
    _impl->lua.safe_script(R"(
        local raw_load = load
        load = function(chunk, name, _, env)
            return raw_load(chunk, name, "t", env)
        end
    )",
                           "=sandbox", sol::load_mode::text);
}

ScriptRuntime::~ScriptRuntime() = default;

bool ScriptRuntime::run_file(const std::filesystem::path& path) noexcept {
    try {
        return report(_impl->lua.safe_script_file(path.string(), &sol::script_pass_on_error,
                                                  sol::load_mode::text));
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", path.string(), exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", path.string());
    }
    return false;
}

bool ScriptRuntime::run_string(std::string_view code, std::string_view chunk_name) noexcept {
    try {
        return report(_impl->lua.safe_script(code, &sol::script_pass_on_error,
                                             "=" + std::string(chunk_name), sol::load_mode::text));
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", chunk_name, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", chunk_name);
    }
    return false;
}

} // namespace engine::script
