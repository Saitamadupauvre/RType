#include "ModuleLoader.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>
#include <utility>

#include "ScriptFiles.hpp"

namespace engine::script {

namespace {

bool is_module_character(char character) {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
           character == '.';
}

bool is_valid_module_name(std::string_view name) {
    return !name.empty() && name.front() != '.' && name.back() != '.' &&
           name.find("..") == std::string_view::npos &&
           std::ranges::all_of(name, is_module_character);
}

std::string module_file(std::string name) {
    std::ranges::replace(name, '.', '/');
    return std::format("{}/{}.lua", library_folder, name);
}

std::string require_chain(const std::vector<std::string>& loading, const std::string& name) {
    std::string chain;
    for (const std::string& module : loading) {
        chain += module + " -> ";
    }
    return chain + name;
}

} // namespace

ModuleLoader::ModuleLoader(std::filesystem::path root) : _root(std::move(root)) {}

sol::object ModuleLoader::require(sol::this_state state, std::string_view requested) {
    std::string name(requested);
    if (!is_valid_module_name(name)) {
        throw std::runtime_error(std::format("invalid module name '{}'", name));
    }
    if (const auto cached = _cache.find(name); cached != _cache.end()) {
        return cached->second;
    }
    if (std::ranges::find(_loading, name) != _loading.end()) {
        throw std::runtime_error(
            std::format("circular require: {}", require_chain(_loading, name)));
    }
    _loading.push_back(name);
    try {
        sol::object value = load(state, name);
        _loading.pop_back();
        _cache.emplace(std::move(name), value);
        return value;
    } catch (...) {
        _loading.pop_back();
        throw;
    }
}

sol::object ModuleLoader::load(sol::this_state state, const std::string& name) {
    const std::string relative = module_file(name);
    const auto code = read_text_file(_root / relative);
    if (!code) {
        throw std::runtime_error(std::format("module '{}' not found at {}", name, relative));
    }
    sol::state_view lua(state);
    const sol::environment environment(lua, sol::create, lua.globals());
    sol::protected_function_result result = lua.safe_script(
        *code, environment, &sol::script_pass_on_error, "@" + relative, sol::load_mode::text);
    if (!result.valid()) {
        const sol::error error = result;
        throw std::runtime_error(std::format("error loading module '{}': {}", name, error.what()));
    }
    if (result.return_count() == 0) {
        return sol::make_object(lua, true);
    }
    sol::object value = result.get<sol::object>();
    if (value.get_type() == sol::type::lua_nil) {
        return sol::make_object(lua, true);
    }
    return value;
}

} // namespace engine::script
