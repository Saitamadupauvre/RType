#include "engine/script/ScriptRuntime.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/physics/Fields.hpp"
#include "loading/Declarations.hpp"
#include "loading/ModuleLoader.hpp"
#include "loading/ScriptFiles.hpp"
#include "loading/Sections.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

core::FieldRegistry engine_fields() {
    core::FieldRegistry fields;
    core::register_core_fields(fields);
    physics::register_physics_fields(fields);
    return fields;
}

std::string_view section_of(const std::vector<std::unique_ptr<ApiSet>>& sets) {
    for (const auto& set : sets) {
        if (!set->section().empty()) {
            return set->section();
        }
    }
    return {};
}

} // namespace

RuntimeContext::RuntimeContext()
    : fields(engine_fields()), types(lua.create_table()), entities(lua, fields) {}

const Declaration* RuntimeContext::find_declaration(std::string_view name) const {
    const auto found = declarations.find(name);
    return found == declarations.end() ? nullptr : &found->second;
}

sol::protected_function RuntimeContext::main_callback(std::string_view name) const {
    if (!main) {
        return sol::lua_nil;
    }
    const auto callback = main->raw_get<sol::object>(name);
    if (callback.get_type() != sol::type::function) {
        return sol::lua_nil;
    }
    return callback.as<sol::protected_function>();
}

struct ScriptRuntime::Impl {
    RuntimeContext context;
    std::shared_ptr<PendingFile> current_file;
    std::optional<ModuleLoader> modules;
    std::vector<std::unique_ptr<ApiSet>> sets;
    std::string_view section;

    explicit Impl(std::vector<std::unique_ptr<ApiSet>> installed);

    void open_sandbox();
    void bind_core();
    sol::object declare(sol::this_state state, DeclarationKind kind, const std::string& name);
    sol::object require(sol::this_state state, const std::string& name);
    bool load_scripts(const fs::path& root);
    bool load_all(const fs::path& root, const std::vector<fs::path>& files);
    bool load_declaration_file(const fs::path& root, const fs::path& relative,
                               const sol::environment& environment);
    bool apply_sections(const std::vector<Declaration>& batch) const;
    bool register_declarations(std::vector<Declaration> batch);
    bool start(const sol::environment& main);
};

ScriptRuntime::Impl::Impl(std::vector<std::unique_ptr<ApiSet>> installed)
    : sets(std::move(installed)), section(section_of(sets)) {
    open_sandbox();
    bind_core();
}

void ScriptRuntime::Impl::open_sandbox() {
    sol::state& lua = context.lua;
    lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                       sol::lib::coroutine);
    lua["dofile"] = sol::lua_nil;
    lua["loadfile"] = sol::lua_nil;
    lua.safe_script(R"(
        local raw_load = load
        load = function(chunk, name, _, env)
            return raw_load(chunk, name, "t", env)
        end
    )",
                    "=sandbox", sol::load_mode::text);
}

void ScriptRuntime::Impl::bind_core() {
    sol::state& lua = context.lua;
    lua.set_function("entity", [this](sol::this_state state, const std::string& name) {
        return declare(state, DeclarationKind::Entity, name);
    });
    lua.set_function("system", [this](sol::this_state state, const std::string& name) {
        return declare(state, DeclarationKind::System, name);
    });
    lua.set_function("require", [this](sol::this_state state, const std::string& name) {
        return require(state, name);
    });
    lua.set_function("sounds", [this](sol::this_state state, const sol::object& names) {
        context.audio.declare_sounds(state, names);
    });
    lua.set_function("musics", [this](sol::this_state state, const sol::object& names) {
        context.audio.declare_musics(state, names);
    });
    lua.set_function("event", [this](sol::this_state state, const sol::object& name) {
        return context.events.declare(state, name);
    });
}

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
    context.audio.allow_declarations(true);
    context.events.allow_declarations(true);
    const bool loaded = load_all(root, files);
    context.events.allow_declarations(false);
    context.audio.allow_declarations(false);
    return loaded;
}

bool ScriptRuntime::Impl::load_all(const fs::path& root, const std::vector<fs::path>& files) {
    bool success = true;
    bool found_entry_point = false;
    std::optional<sol::environment> entry;
    for (const fs::path& relative : files) {
        const sol::environment environment(context.lua, sol::create, context.lua.globals());
        const bool loaded = load_declaration_file(root, relative, environment);
        success = loaded && success;
        if (relative == fs::path(entry_point)) {
            found_entry_point = true;
            entry = loaded ? std::optional(environment) : std::nullopt;
        }
    }
    if (!found_entry_point) {
        log::error("lua: {} has no {}", root.string(), entry_point);
        return false;
    }
    return entry && start(*entry) && success;
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
        if (!report(context.lua.safe_script(*code, environment, &sol::script_pass_on_error,
                                            "@" + source, sol::load_mode::text))) {
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
    return resolved && apply_sections(*resolved) && register_declarations(std::move(*resolved));
}

bool ScriptRuntime::Impl::apply_sections(const std::vector<Declaration>& batch) const {
    return std::ranges::all_of(batch, [this](const Declaration& declaration) {
        return apply_section(declaration, section);
    });
}

bool ScriptRuntime::Impl::register_declarations(std::vector<Declaration> batch) {
    for (auto current = batch.begin(); current != batch.end(); ++current) {
        const auto same_name = [&](const Declaration& other) {
            return other.name == current->name;
        };
        const auto earlier = std::find_if(batch.begin(), current, same_name);
        const auto registered = context.declarations.find(current->name);
        if (earlier == current && registered == context.declarations.end()) {
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
        context.declarations.emplace(std::move(name), std::move(declaration));
    }
    return true;
}

bool ScriptRuntime::Impl::start(const sol::environment& environment) {
    context.main = environment;
    const auto on_start = environment.raw_get<sol::object>("on_start");
    if (on_start.get_type() != sol::type::function) {
        log::warn("lua: {} does not define on_start()", entry_point);
        return true;
    }
    return report(on_start.as<sol::protected_function>()());
}

ScriptRuntimeBuilder ScriptRuntime::builder() { return {}; }

ScriptRuntime::ScriptRuntime(std::vector<std::unique_ptr<ApiSet>> sets)
    : _impl(std::make_unique<Impl>(std::move(sets))) {
    for (const auto& set : _impl->sets) {
        set->install(_impl->context);
    }
}

ScriptRuntime::~ScriptRuntime() = default;

ScriptRuntime::ScriptRuntime(ScriptRuntime&&) noexcept = default;

ScriptRuntime& ScriptRuntime::operator=(ScriptRuntime&&) noexcept = default;

ApiSet& ScriptRuntime::find_api(const std::type_info& type) const {
    for (const auto& set : _impl->sets) {
        if (typeid(*set) == type) {
            return *set;
        }
    }
    throw std::out_of_range(std::format("script API set {} is not installed", type.name()));
}

bool ScriptRuntime::run_file(const std::filesystem::path& path) noexcept {
    try {
        return report(_impl->context.lua.safe_script_file(path.string(), &sol::script_pass_on_error,
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
        return report(_impl->context.lua.safe_script(
            code, &sol::script_pass_on_error, "=" + std::string(chunk_name), sol::load_mode::text));
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
    const Declaration* found = _impl->context.find_declaration(name);
    if (found == nullptr) {
        return std::nullopt;
    }
    return DeclarationInfo{.kind = found->kind, .source = found->source};
}

void ScriptRuntime::attach_world(core::EntityManager& world) {
    _impl->context.entities.attach(world);
}

const core::FieldRegistry& ScriptRuntime::fields() const noexcept { return _impl->context.fields; }

std::optional<core::Entity> ScriptRuntime::spawn(std::string_view type,
                                                 std::optional<core::Vec2> position) noexcept {
    try {
        const Declaration* found = _impl->context.find_declaration(type);
        if (found == nullptr) {
            log::error("lua: cannot spawn unknown entity '{}'", type);
            return std::nullopt;
        }
        const auto entity = _impl->context.entities.spawn(*found, position);
        if (entity) {
            _impl->context.entities.finish_spawn(*entity);
        }
        return entity;
    } catch (const std::exception& exception) {
        log::error("lua: cannot spawn '{}': {}", type, exception.what());
    } catch (...) {
        log::error("lua: cannot spawn '{}': unknown error", type);
    }
    return std::nullopt;
}

void ScriptRuntime::release(core::Entity entity) noexcept {
    try {
        _impl->context.entities.release(entity);
    } catch (const std::exception& exception) {
        log::error("lua: release: {}", exception.what());
    } catch (...) {
        log::error("lua: release: unknown error");
    }
}

bool ScriptRuntime::call(core::Entity entity, std::string_view method) noexcept {
    try {
        return _impl->context.entities.call(entity, method);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", method, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", method);
    }
    return false;
}

std::vector<std::string> ScriptRuntime::entity_types() const {
    std::vector<std::string> types;
    for (const auto& [name, declaration] : _impl->context.declarations) {
        if (declaration.kind == DeclarationKind::Entity) {
            types.push_back(name);
        }
    }
    return types;
}

std::vector<std::string> ScriptRuntime::sound_names() const {
    return _impl->context.audio.sound_names();
}

std::vector<std::string> ScriptRuntime::music_names() const {
    return _impl->context.audio.music_names();
}

std::vector<std::string> ScriptRuntime::event_names() const {
    return _impl->context.events.names();
}

std::vector<std::string> ScriptRuntime::event_keys() const { return _impl->context.events.keys(); }

std::optional<std::string> ScriptRuntime::entity_type(core::Entity entity) const {
    return _impl->context.entities.type_of(entity);
}

void ScriptRuntime::update(float dt) noexcept {
    try {
        _impl->context.entities.update(dt);
    } catch (const std::exception& exception) {
        log::error("lua: update: {}", exception.what());
    } catch (...) {
        log::error("lua: update: unknown error");
    }
}

void ScriptRuntime::collide(core::Entity first, core::Entity second) noexcept {
    try {
        _impl->context.entities.collide(first, second);
    } catch (const std::exception& exception) {
        log::error("lua: on_collide: {}", exception.what());
    } catch (...) {
        log::error("lua: on_collide: unknown error");
    }
}

std::size_t ScriptRuntime::flush_destroyed() noexcept {
    try {
        return _impl->context.entities.flush_destroyed();
    } catch (const std::exception& exception) {
        log::error("lua: flush_destroyed: {}", exception.what());
    } catch (...) {
        log::error("lua: flush_destroyed: unknown error");
    }
    return 0;
}

} // namespace engine::script
