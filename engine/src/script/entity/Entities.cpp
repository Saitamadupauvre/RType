#include "Entities.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/physics/Components.hpp"
#include "loading/Declarations.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::array<std::string_view, 3> read_only_fields{"id", "type", "age"};

bool is_read_only(std::string_view name) {
    return std::ranges::find(read_only_fields, name) != read_only_fields.end();
}

bool is_nil(const sol::object& value) {
    return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
}

template <typename Component> void register_if_missing(core::EntityManager& world) {
    if (!world.is_registered<Component>()) {
        world.register_component<Component>();
    }
}

void register_script_components(core::EntityManager& world) {
    register_if_missing<core::Position>(world);
    register_if_missing<core::Sprite>(world);
    register_if_missing<core::Tags>(world);
    register_if_missing<physics::CollidesWith>(world);
    register_if_missing<physics::AABBCollider>(world);
    register_if_missing<physics::PhysicsBody>(world);
    register_if_missing<physics::CircleCollider>(world);
    register_if_missing<physics::Scriptable>(world);
}

constexpr std::size_t max_table_depth = 64;

using TableCopies = std::unordered_map<const void*, sol::table>;

sol::table copy_table(sol::state& lua, const sol::table& source, TableCopies& copies,
                      std::size_t depth) {
    if (const auto found = copies.find(source.pointer()); found != copies.end()) {
        return found->second;
    }
    if (depth >= max_table_depth) {
        throw std::runtime_error(
            std::format("prefab table nests deeper than {} levels", max_table_depth));
    }
    sol::table copy = lua.create_table();
    copies.emplace(source.pointer(), copy);
    for (const auto& [key, value] : source) {
        if (value.get_type() == sol::type::table) {
            copy.raw_set(key, copy_table(lua, value.as<sol::table>(), copies, depth + 1));
        } else {
            copy.raw_set(key, value);
        }
    }
    const sol::optional<sol::table> metatable = source[sol::metatable_key];
    if (metatable) {
        copy[sol::metatable_key] = *metatable;
    }
    return copy;
}

} // namespace

Entities::Entities(sol::state& lua, const core::FieldRegistry& fields)
    : _lua(lua), _fields(fields), _methods(lua.create_table()) {
    sol::table types = _lua.create_table();
    types.new_usertype<EntityHandle>(
        "EntityHandle", sol::no_constructor, sol::meta_function::index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key) {
            return get(state, handle, key);
        },
        sol::meta_function::new_index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key,
               const sol::object& value) { set(state, handle, key, value); });
    _methods.set_function("destroy", [this](sol::this_state state, const EntityHandle& handle) {
        destroy(state, handle);
    });
    _methods.set_function("has_tag",
                          [this](sol::this_state state, const EntityHandle& handle,
                                 std::string_view label) { return has_tag(state, handle, label); });
    _methods.set_function("get_field", [this](sol::this_state state, const EntityHandle& handle,
                                              std::string_view component, std::string_view field) {
        return get_field(state, handle, component, field);
    });
    _methods.set_function("set_field", [this](sol::this_state state, const EntityHandle& handle,
                                              std::string_view component, std::string_view field,
                                              const sol::object& value) {
        set_field(state, handle, component, field, value);
    });
}

void Entities::add_handle_field(std::string name, HandleField field) {
    _handle_fields.insert_or_assign(std::move(name), std::move(field));
}

void Entities::on_release(std::function<void(core::Entity)> listener) {
    _release_listeners.push_back(std::move(listener));
}

void Entities::attach(core::EntityManager& world) {
    register_script_components(world);
    _world = &world;
}

core::Entity Entities::prototype(core::EntityManager& scratch, const Declaration& declaration) {
    register_script_components(scratch);
    const core::Entity entity = scratch.spawn();
    scratch.add(entity, core::Position{});
    try {
        apply_prefab_fields(scratch, entity, declaration.prefab);
    } catch (...) {
        scratch.kill(entity);
        throw;
    }
    return entity;
}

std::optional<core::Entity> Entities::spawn(const Declaration& declaration,
                                            std::optional<core::Vec2> position) {
    if (_world == nullptr) {
        log::error("lua: cannot spawn '{}': no world is attached", declaration.name);
        return std::nullopt;
    }
    if (declaration.kind != DeclarationKind::Entity) {
        log::error("lua: cannot spawn '{}': it is declared as a {}", declaration.name,
                   kind_name(declaration.kind));
        return std::nullopt;
    }
    const core::Entity entity = _world->spawn();
    _world->add(entity, core::Position{});
    sol::table data;
    try {
        apply_prefab_fields(*_world, entity, declaration.prefab);
        data = own_copies(declaration.prefab);
    } catch (const std::exception& error) {
        _world->kill(entity);
        log::error("lua: cannot spawn '{}': {}", declaration.name, error.what());
        return std::nullopt;
    }
    if (position) {
        _world->get<core::Position>(entity).pos = *position;
    }
    _records.insert_or_assign(entity.id(),
                              Record{.type = declaration.name,
                                     .data = std::move(data),
                                     .prefab = declaration.prefab,
                                     .handle = sol::make_object(_lua, EntityHandle{entity})});
    return entity;
}

sol::object Entities::spawn_from_lua(sol::this_state state, const Declaration& declaration,
                                     const sol::optional<sol::table>& overrides) {
    const auto entity = spawn(declaration, std::nullopt);
    if (!entity) {
        return sol::make_object(state, sol::lua_nil);
    }
    sol::object handle = _records.at(entity->id()).handle;
    if (overrides) {
        try {
            overrides->for_each([&](const sol::object& key, const sol::object& value) {
                set(state, handle.as<EntityHandle>(), key, value);
            });
        } catch (...) {
            release(*entity);
            _world->kill(*entity);
            throw;
        }
    }
    finish_spawn(*entity);
    return handle;
}

void Entities::finish_spawn(core::Entity entity) { invoke(entity, "on_spawn"); }

void Entities::release(core::Entity entity) {
    if (_records.erase(entity.id()) == 0) {
        return;
    }
    for (const auto& listener : _release_listeners) {
        listener(entity);
    }
}

bool Entities::call(core::Entity entity, std::string_view method) {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(entity)) {
        log::error("lua: cannot call '{}' on destroyed entity (id {})", method, entity.id());
        return false;
    }
    return invoke(entity, method);
}

void Entities::update(float dt) {
    for (const core::Entity entity : live_entities()) {
        const auto found = _records.find(entity.id());
        if (found == _records.end() || found->second.doomed || !_world->alive(entity)) {
            continue;
        }
        found->second.age += dt;
        invoke(entity, "update", dt);
    }
}

std::size_t Entities::flush_destroyed() {
    std::size_t destroyed = 0;
    while (!_doomed.empty()) {
        for (const core::Entity entity : std::exchange(_doomed, {})) {
            if (!_records.contains(entity.id())) {
                continue;
            }
            invoke(entity, "on_destroy");
            release(entity);
            if (_world->alive(entity)) {
                _world->kill(entity);
            }
            ++destroyed;
        }
    }
    return destroyed;
}

void Entities::collide(core::Entity first, core::Entity second) {
    if (!is_active(first) || !is_active(second)) {
        return;
    }
    if (wants_collision(first, second)) {
        invoke(first, "on_collide", _records.at(second.id()).handle);
    }
    if (wants_collision(second, first)) {
        invoke(second, "on_collide", _records.at(first.id()).handle);
    }
}

bool Entities::is_active(core::Entity entity) const {
    const auto found = _records.find(entity.id());
    return found != _records.end() && !found->second.doomed && _world != nullptr &&
           _world->alive(entity);
}

bool Entities::wants_collision(core::Entity entity, core::Entity other) const {
    if (!_records.contains(entity.id()) || !_records.contains(other.id()) ||
        !_world->alive(entity) || !_world->alive(other) ||
        !_world->has<physics::CollidesWith>(entity) || !_world->has<core::Tags>(other)) {
        return false;
    }
    const core::Tags& tags = _world->get<core::Tags>(other);
    return std::ranges::any_of(_world->get<physics::CollidesWith>(entity).tags.labels(),
                               [&](const std::string& label) { return tags.has(label); });
}

template <typename... Args>
bool Entities::invoke(core::Entity entity, std::string_view method, Args&&... args) {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(entity)) {
        return false;
    }
    const auto function = found->second.prefab.raw_get<sol::object>(method);
    if (function.get_type() != sol::type::function) {
        return false;
    }
    const sol::object self = found->second.handle;
    return report(function.as<sol::protected_function>()(self, std::forward<Args>(args)...));
}

sol::object Entities::get(sol::this_state state, const EntityHandle& handle,
                          const sol::object& key) {
    const Record& record = live_record(state, handle);
    if (key.get_type() == sol::type::string) {
        const auto name = key.as<std::string_view>();
        if (name == "id") {
            return sol::make_object(state, static_cast<lua_Integer>(handle.entity.id()));
        }
        if (name == "type") {
            return sol::make_object(state, record.type);
        }
        if (name == "age") {
            return sol::make_object(state, record.age);
        }
        if (const auto field = _handle_fields.find(name); field != _handle_fields.end()) {
            return field->second.read(state, handle.entity);
        }
        if (_fields.has_alias(name)) {
            return _fields.read_alias(state, *_world, handle.entity, name);
        }
        auto method = _methods.raw_get<sol::object>(name);
        if (!is_nil(method)) {
            return method;
        }
    }
    auto own = record.data.raw_get<sol::object>(key);
    if (!is_nil(own)) {
        return own;
    }
    return record.prefab.raw_get<sol::object>(key);
}

void Entities::set(sol::this_state state, const EntityHandle& handle, const sol::object& key,
                   const sol::object& value) {
    Record& record = live_record(state, handle);
    if (key.get_type() == sol::type::string) {
        const auto name = key.as<std::string_view>();
        if (is_read_only(name)) {
            throw std::runtime_error(
                std::format("{}entity field '{}' is read-only", caller_location(state), name));
        }
        if (const auto field = _handle_fields.find(name); field != _handle_fields.end()) {
            if (!field->second.write) {
                throw std::runtime_error(
                    std::format("{}entity field '{}' is read-only", caller_location(state), name));
            }
            field->second.write(state, handle.entity, value);
            return;
        }
        if (_fields.has_alias(name)) {
            _fields.write_alias(state, *_world, handle.entity, name, value);
            return;
        }
    }
    if (is_nil(key)) {
        throw std::runtime_error(
            std::format("{}entity field name must not be nil", caller_location(state)));
    }
    record.data.raw_set(key, value);
}

void Entities::apply_prefab_fields(core::EntityManager& world, core::Entity entity,
                                   const sol::table& prefab) {
    const sol::this_state state{_lua.lua_state()};
    for (const std::string_view name : _fields.alias_names()) {
        const auto value = prefab.raw_get<sol::object>(name);
        if (!is_nil(value)) {
            _fields.write_alias(state, world, entity, name, value);
        }
    }
}

sol::table Entities::own_copies(const sol::table& prefab) {
    sol::table data = _lua.create_table();
    TableCopies copies;
    for (const auto& [key, value] : prefab) {
        const bool alias =
            key.get_type() == sol::type::string && _fields.has_alias(key.as<std::string_view>());
        if (!alias && value.get_type() == sol::type::table) {
            data.raw_set(key, copy_table(_lua, value.as<sol::table>(), copies, 0));
        }
    }
    return data;
}

Entities::Record& Entities::live_record(sol::this_state state, const EntityHandle& handle) {
    const auto found = _records.find(handle.entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(handle.entity)) {
        throw std::runtime_error(std::format("{}attempt to use a destroyed entity (id {})",
                                             caller_location(state), handle.entity.id()));
    }
    return found->second;
}

void Entities::destroy(sol::this_state state, const EntityHandle& handle) {
    live_record(state, handle);
    doom(handle.entity);
}

void Entities::doom(core::Entity entity) {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || found->second.doomed) {
        return;
    }
    found->second.doomed = true;
    _doomed.push_back(entity);
}

std::optional<std::string> Entities::type_of(core::Entity entity) const {
    const auto found = _records.find(entity.id());
    if (found == _records.end()) {
        return std::nullopt;
    }
    return found->second.type;
}

std::optional<std::string> Entities::name_of(core::Entity entity) const {
    const auto found = _records.find(entity.id());
    if (found == _records.end()) {
        return std::nullopt;
    }
    return found->second.name;
}

bool Entities::rename(core::Entity entity, std::string name) {
    const auto found = _records.find(entity.id());
    if (found == _records.end()) {
        return false;
    }
    found->second.name = std::move(name);
    return true;
}

sol::object Entities::handle_of(core::Entity entity) const {
    return _records.at(entity.id()).handle;
}

bool Entities::has_tag(sol::this_state state, const EntityHandle& handle, std::string_view label) {
    live_record(state, handle);
    return _world->has<core::Tags>(handle.entity) &&
           _world->get<core::Tags>(handle.entity).has(label);
}

sol::object Entities::get_field(sol::this_state state, const EntityHandle& handle,
                                std::string_view component, std::string_view field) {
    live_record(state, handle);
    return _fields.read(state, *_world, handle.entity, component, field);
}

void Entities::set_field(sol::this_state state, const EntityHandle& handle,
                         std::string_view component, std::string_view field,
                         const sol::object& value) {
    live_record(state, handle);
    _fields.write(state, *_world, handle.entity, component, field, value);
}

std::vector<core::Entity> Entities::live_entities() const {
    std::vector<core::Entity::Id> ids;
    ids.reserve(_records.size());
    for (const auto& [id, record] : _records) {
        ids.push_back(id);
    }
    std::ranges::sort(ids);
    std::vector<core::Entity> entities;
    entities.reserve(ids.size());
    for (const core::Entity::Id id : ids) {
        entities.emplace_back(id);
    }
    return entities;
}

} // namespace engine::script
