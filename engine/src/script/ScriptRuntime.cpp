#include "engine/script/ScriptRuntime.hpp"

#include <cstdint>
#include <exception>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Audio.hpp"
#include "Declarations.hpp"
#include "Entities.hpp"
#include "ModuleLoader.hpp"
#include "Players.hpp"
#include "ScriptFiles.hpp"
#include "Sol.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace fs = std::filesystem;
namespace log = engine::core::log;

constexpr std::string_view on_player_joined = "on_player_joined";
constexpr std::string_view on_player_left = "on_player_left";

} // namespace

struct ScriptRuntime::Impl {
    sol::state lua;
    Players players;
    Entities entities{lua, players};
    std::optional<sol::environment> main;
    std::map<std::string, Declaration, std::less<>> declarations;
    std::shared_ptr<PendingFile> current_file;
    std::optional<ModuleLoader> modules;
    Audio audio;

    sol::object declare(sol::this_state state, DeclarationKind kind, const std::string& name);
    sol::object require(sol::this_state state, const std::string& name);
    sol::object spawn(sol::this_state state, const std::string& type,
                      const sol::optional<sol::table>& overrides);
    bool load_scripts(const fs::path& root);
    bool load_all(const fs::path& root, const std::vector<fs::path>& files);
    bool load_declaration_file(const fs::path& root, const fs::path& relative,
                               const sol::environment& environment);
    bool register_declarations(std::vector<Declaration> batch);
    bool start(const sol::environment& main);
    [[nodiscard]] sol::protected_function main_callback(std::string_view name) const;
    sol::table player_entities(sol::this_state state, const PlayerHandle& player) const;
    sol::object player_field(sol::this_state state, const PlayerHandle& player,
                             const std::string& key) const;
    sol::table all_players(sol::this_state state) const;
    bool player_joined(std::uint32_t player_id);
    bool player_left(std::uint32_t player_id);
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

sol::object ScriptRuntime::Impl::spawn(sol::this_state state, const std::string& type,
                                       const sol::optional<sol::table>& overrides) {
    const auto found = declarations.find(type);
    if (found == declarations.end()) {
        log::error("lua: {}cannot spawn unknown entity '{}'", caller_location(state), type);
        return sol::make_object(state, sol::lua_nil);
    }
    return entities.spawn_from_lua(state, found->second, overrides);
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
    audio.allow_declarations(true);
    const bool loaded = load_all(root, files);
    audio.allow_declarations(false);
    return loaded;
}

bool ScriptRuntime::Impl::load_all(const fs::path& root, const std::vector<fs::path>& files) {
    bool success = true;
    bool found_entry_point = false;
    std::optional<sol::environment> entry;
    for (const fs::path& relative : files) {
        const sol::environment environment(lua, sol::create, lua.globals());
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

bool ScriptRuntime::Impl::start(const sol::environment& environment) {
    main = environment;
    const auto on_start = environment.raw_get<sol::object>("on_start");
    if (on_start.get_type() != sol::type::function) {
        log::warn("lua: {} does not define on_start()", entry_point);
        return true;
    }
    return report(on_start.as<sol::protected_function>()());
}

sol::protected_function ScriptRuntime::Impl::main_callback(std::string_view name) const {
    if (!main) {
        return sol::lua_nil;
    }
    const auto callback = main->raw_get<sol::object>(name);
    if (callback.get_type() != sol::type::function) {
        return sol::lua_nil;
    }
    return callback.as<sol::protected_function>();
}

sol::table ScriptRuntime::Impl::player_entities(sol::this_state state,
                                                const PlayerHandle& player) const {
    sol::table owned = sol::state_view(state).create_table();
    for (const core::Entity entity : entities.owned_by(player.id)) {
        owned.add(entities.handle_of(entity));
    }
    return owned;
}

sol::object ScriptRuntime::Impl::player_field(sol::this_state state, const PlayerHandle& player,
                                              const std::string& key) const {
    if (key == "id") {
        return sol::make_object(state, static_cast<lua_Integer>(player.id));
    }
    if (key == "slot") {
        const auto slot = players.slot(player.id);
        return slot ? sol::make_object(state, static_cast<lua_Integer>(*slot))
                    : sol::make_object(state, sol::lua_nil);
    }
    if (key == "connected") {
        return sol::make_object(state, players.contains(player.id));
    }
    if (key == "entities") {
        return sol::make_object(state, [this](sol::this_state inner, const PlayerHandle& self) {
            return player_entities(inner, self);
        });
    }
    return sol::make_object(state, sol::lua_nil);
}

sol::table ScriptRuntime::Impl::all_players(sol::this_state state) const {
    sol::table result = sol::state_view(state).create_table();
    for (const std::uint32_t id : players.ids()) {
        result.add(PlayerHandle{id});
    }
    return result;
}

bool ScriptRuntime::Impl::player_joined(std::uint32_t player_id) {
    if (!players.join(player_id)) {
        log::warn("lua: player {} is already connected", player_id);
        return false;
    }
    const auto callback = main_callback(on_player_joined);
    if (!callback.valid()) {
        return true;
    }
    return report(callback(PlayerHandle{player_id}));
}

bool ScriptRuntime::Impl::player_left(std::uint32_t player_id) {
    if (!players.contains(player_id)) {
        log::warn("lua: unknown player {} left", player_id);
        return false;
    }
    const auto callback = main_callback(on_player_left);
    bool success = true;
    if (callback.valid()) {
        success = report(callback(PlayerHandle{player_id}));
    } else {
        for (const core::Entity entity : entities.owned_by(player_id)) {
            entities.doom(entity);
        }
    }
    players.leave(player_id);
    return success;
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
    _impl->lua.set_function("spawn", [impl](sol::this_state state, const std::string& type,
                                            const sol::optional<sol::table>& overrides) {
        return impl->spawn(state, type, overrides);
    });
    sol::table types = _impl->lua.create_table();
    types.new_usertype<PlayerHandle>(
        "PlayerHandle", sol::no_constructor, sol::meta_function::index,
        [impl](sol::this_state state, const PlayerHandle& player, const std::string& key) {
            return impl->player_field(state, player, key);
        },
        sol::meta_function::equal_to,
        [](const PlayerHandle& left, const PlayerHandle& right) { return left.id == right.id; });
    _impl->lua.set_function("players",
                            [impl](sol::this_state state) { return impl->all_players(state); });
    _impl->lua.set_function("find_player", [impl](sol::this_state state, lua_Integer id) {
        return impl->entities.player_object(state, static_cast<std::uint32_t>(id));
    });
    _impl->lua.set_function("require", [impl](sol::this_state state, const std::string& name) {
        return impl->require(state, name);
    });
    _impl->lua.set_function("sounds", [impl](sol::this_state state, const sol::object& names) {
        impl->audio.declare_sounds(state, names);
    });
    _impl->lua.set_function("musics", [impl](sol::this_state state, const sol::object& names) {
        impl->audio.declare_musics(state, names);
    });
    _impl->lua.set_function(
        "play", [impl](sol::this_state state, const sol::object& name, const sol::object& source) {
            impl->audio.play(state, name, source);
        });
    _impl->lua.set_function("music", [impl](sol::this_state state, const sol::object& name) {
        impl->audio.music(state, name);
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

void ScriptRuntime::attach_world(core::EntityManager& world) { _impl->entities.attach(world); }

std::optional<core::Entity> ScriptRuntime::spawn(std::string_view type,
                                                 std::optional<core::Vec2> position) noexcept {
    try {
        const auto found = _impl->declarations.find(type);
        if (found == _impl->declarations.end()) {
            log::error("lua: cannot spawn unknown entity '{}'", type);
            return std::nullopt;
        }
        const auto entity = _impl->entities.spawn(found->second, position);
        if (entity) {
            _impl->entities.finish_spawn(*entity);
        }
        return entity;
    } catch (const std::exception& exception) {
        log::error("lua: cannot spawn '{}': {}", type, exception.what());
    } catch (...) {
        log::error("lua: cannot spawn '{}': unknown error", type);
    }
    return std::nullopt;
}

void ScriptRuntime::release(core::Entity entity) noexcept { _impl->entities.release(entity); }

bool ScriptRuntime::call(core::Entity entity, std::string_view method) noexcept {
    try {
        return _impl->entities.call(entity, method);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", method, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", method);
    }
    return false;
}

void ScriptRuntime::set_input(core::Entity entity, InputState input) noexcept {
    _impl->entities.set_input(entity, std::move(input));
}

bool ScriptRuntime::player_joined(std::uint32_t player_id) noexcept {
    try {
        return _impl->player_joined(player_id);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", on_player_joined, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", on_player_joined);
    }
    return false;
}

bool ScriptRuntime::player_left(std::uint32_t player_id) noexcept {
    try {
        return _impl->player_left(player_id);
    } catch (const std::exception& exception) {
        log::error("lua: {}: {}", on_player_left, exception.what());
    } catch (...) {
        log::error("lua: {}: unknown error", on_player_left);
    }
    return false;
}

void ScriptRuntime::set_player_input(std::uint32_t player_id, InputState input) noexcept {
    _impl->players.set_input(player_id, std::move(input));
}

std::vector<std::string> ScriptRuntime::entity_types() const {
    std::vector<std::string> types;
    for (const auto& [name, declaration] : _impl->declarations) {
        if (declaration.kind == DeclarationKind::Entity) {
            types.push_back(name);
        }
    }
    return types;
}

std::vector<std::string> ScriptRuntime::sound_names() const { return _impl->audio.sound_names(); }

std::vector<std::string> ScriptRuntime::music_names() const { return _impl->audio.music_names(); }

std::vector<AudioCommand> ScriptRuntime::take_audio_commands() noexcept {
    return _impl->audio.take_commands();
}

std::optional<std::string> ScriptRuntime::entity_type(core::Entity entity) const {
    return _impl->entities.type_of(entity);
}

std::optional<std::uint32_t> ScriptRuntime::entity_owner(core::Entity entity) const {
    return _impl->entities.owner_of(entity);
}

void ScriptRuntime::update(float dt) noexcept {
    try {
        _impl->entities.update(dt);
    } catch (const std::exception& exception) {
        log::error("lua: update: {}", exception.what());
    } catch (...) {
        log::error("lua: update: unknown error");
    }
}

void ScriptRuntime::collide(core::Entity first, core::Entity second) noexcept {
    try {
        _impl->entities.collide(first, second);
    } catch (const std::exception& exception) {
        log::error("lua: on_collide: {}", exception.what());
    } catch (...) {
        log::error("lua: on_collide: unknown error");
    }
}

std::size_t ScriptRuntime::flush_destroyed() noexcept {
    try {
        return _impl->entities.flush_destroyed();
    } catch (const std::exception& exception) {
        log::error("lua: flush_destroyed: {}", exception.what());
    } catch (...) {
        log::error("lua: flush_destroyed: unknown error");
    }
    return 0;
}

} // namespace engine::script
