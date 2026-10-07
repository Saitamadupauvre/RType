#include "engine/script/ScriptRuntime.hpp"

#include <exception>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Declarations.hpp"
#include "ModuleLoader.hpp"
#include "ScriptFiles.hpp"
#include "Sol.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
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
    std::map<std::string, Declaration, std::less<>> declarations;
    std::shared_ptr<PendingFile> current_file;
    std::optional<ModuleLoader> modules;

    sol::object declare(sol::this_state state, DeclarationKind kind, const std::string& name);
    sol::object require(sol::this_state state, const std::string& name);
    bool load_scripts(const fs::path& root);
    bool load_declaration_file(const fs::path& root, const fs::path& relative,
                               const sol::environment& environment);
    bool register_declarations(std::vector<Declaration> batch);
    bool start(const sol::environment& main);
};

sol::object ScriptRuntime::Impl::declare(sol::this_state state, DeclarationKind kind,
                                         const std::string& name) {
    if (!current_file) {
        throw std::runtime_error(
            std::format("{}{} '{}' must be declared by a script file outside {}/",
                        caller_location(state), kind_name(kind), name, library_folder));
    }
    if (name.empty()) {
        throw std::runtime_error(
            std::format("{}{} name must not be empty", caller_location(state), kind_name(kind)));
    }
    current_file->declarations.push_back(
        PendingDeclaration{.name = name, .kind = kind, .body = std::nullopt});
    return make_declarator(state, current_file, current_file->declarations.size() - 1);
}

sol::object ScriptRuntime::Impl::require(sol::this_state state, const std::string& name) {
    if (!modules) {
        throw std::runtime_error(
            std::format("{}require is only available to loaded scripts", caller_location(state)));
    }
    const DeclarationScope suspended(current_file, nullptr);
    try {
        return modules->require(state, name);
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(caller_location(state) + error.what());
    }
}

bool ScriptRuntime::Impl::load_scripts(const fs::path& root) {
    if (modules) {
        log::error("lua: scripts are already loaded");
        return false;
    }
    std::error_code error;
    if (!fs::is_directory(root, error)) {
        log::error("lua: script folder {} does not exist", root.string());
        return false;
    }
    const std::vector<fs::path> files = collect_script_files(root, error);
    if (error) {
        log::error("lua: cannot list {}: {}", root.string(), error.message());
        return false;
    }
    modules.emplace(root);
    bool success = true;
    bool found_entry_point = false;
    std::optional<sol::environment> main;
    for (const fs::path& relative : files) {
        const sol::environment environment(lua, sol::create, lua.globals());
        const bool loaded = load_declaration_file(root, relative, environment);
        success = loaded && success;
        if (relative == fs::path(entry_point)) {
            found_entry_point = true;
            main = loaded ? std::optional(environment) : std::nullopt;
        }
    }
    if (!found_entry_point) {
        log::error("lua: {} has no {}", root.string(), entry_point);
        return false;
    }
    return main && start(*main) && success;
}

bool ScriptRuntime::Impl::load_declaration_file(const fs::path& root, const fs::path& relative,
                                                const sol::environment& environment) {
    const std::string source = relative.generic_string();
    const auto code = read_text_file(root / relative);
    if (!code) {
        log::error("lua: cannot read {}", source);
        return false;
    }
    auto file = std::make_shared<PendingFile>(PendingFile{.source = relative, .declarations = {}});
    {
        const DeclarationScope scope(current_file, file);
        if (!report(lua.safe_script(*code, environment, &sol::script_pass_on_error, "@" + source,
                                    sol::load_mode::text))) {
            file->declarations.clear();
            return false;
        }
    }
    if (file->declarations.empty()) {
        if (relative != fs::path(entry_point)) {
            log::warn("lua: {} declares nothing", source);
        }
        return true;
    }
    auto resolved = resolve_declarations(*file, environment);
    file->declarations.clear();
    return resolved && register_declarations(std::move(*resolved));
}

bool ScriptRuntime::Impl::register_declarations(std::vector<Declaration> batch) {
    for (auto current = batch.begin(); current != batch.end(); ++current) {
        const auto same_name = [&](const Declaration& other) {
            return other.name == current->name;
        };
        const auto earlier = std::find_if(batch.begin(), current, same_name);
        const auto registered = declarations.find(current->name);
        if (earlier == current && registered == declarations.end()) {
            continue;
        }
        const fs::path& first = earlier != current ? earlier->source : registered->second.source;
        log::error("lua: duplicate declaration '{}' in {}, first declared in {}", current->name,
                   current->source.generic_string(), first.generic_string());
        return false;
    }
    for (Declaration& declaration : batch) {
        warn_reserved_names(declaration);
        std::string name = declaration.name;
        declarations.emplace(std::move(name), std::move(declaration));
    }
    return true;
}

bool ScriptRuntime::Impl::start(const sol::environment& main) {
    const auto on_start = main.raw_get<sol::object>("on_start");
    if (on_start.get_type() != sol::type::function) {
        log::warn("lua: {} does not define on_start()", entry_point);
        return true;
    }
    return report(on_start.as<sol::protected_function>()());
}

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
    Impl* impl = _impl.get();
    _impl->lua.set_function("entity", [impl](sol::this_state state, const std::string& name) {
        return impl->declare(state, DeclarationKind::Entity, name);
    });
    _impl->lua.set_function("system", [impl](sol::this_state state, const std::string& name) {
        return impl->declare(state, DeclarationKind::System, name);
    });
    _impl->lua.set_function("require", [impl](sol::this_state state, const std::string& name) {
        return impl->require(state, name);
    });
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

bool ScriptRuntime::load_scripts(const std::filesystem::path& root) noexcept {
    try {
        return _impl->load_scripts(root);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", root.string(), exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", root.string());
    }
    return false;
}

std::optional<DeclarationInfo> ScriptRuntime::find_declaration(std::string_view name) const {
    const auto found = _impl->declarations.find(name);
    if (found == _impl->declarations.end()) {
        return std::nullopt;
    }
    return DeclarationInfo{.kind = found->second.kind, .source = found->second.source};
}

} // namespace engine::script
