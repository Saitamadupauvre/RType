#include "Entities.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string_view>

#include "Declarations.hpp"
#include "NativeFields.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/log/Log.hpp"

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

} // namespace

Entities::Entities(sol::state& lua) : _lua(lua) {
    sol::table types = _lua.create_table();
    types.new_usertype<EntityHandle>(
        "EntityHandle", sol::no_constructor, sol::meta_function::index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key) {
            return get(state, handle, key);
        },
        sol::meta_function::new_index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key,
               const sol::object& value) { set(state, handle, key, value); });
}

void Entities::attach(core::EntityManager& world) {
    register_if_missing<core::Position>(world);
    register_if_missing<core::Velocity>(world);
    _world = &world;
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
    _world->add(entity, core::Velocity{});
    try {
        apply_prefab_fields(entity, declaration.prefab);
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
                                     .data = _lua.create_table(),
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
    return handle;
}

void Entities::release(core::Entity entity) { _records.erase(entity.id()); }

bool Entities::call(core::Entity entity, std::string_view method) {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(entity)) {
        log::error("lua: cannot call '{}' on destroyed entity (id {})", method, entity.id());
        return false;
    }
    const auto function = found->second.prefab.raw_get<sol::object>(method);
    if (function.get_type() != sol::type::function) {
        return false;
    }
    const sol::object self = found->second.handle;
    return report(function.as<sol::protected_function>()(self));
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
        if (const auto field = find_native_field(name)) {
            return field->read(state, *_world, handle.entity);
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
        if (const auto field = find_native_field(name)) {
            field->write(state, *_world, handle.entity, value);
            return;
        }
    }
    if (is_nil(key)) {
        throw std::runtime_error(
            std::format("{}entity field name must not be nil", caller_location(state)));
    }
    record.data.raw_set(key, value);
}

void Entities::apply_prefab_fields(core::Entity entity, const sol::table& prefab) {
    const sol::this_state state{_lua.lua_state()};
    for (const NativeField& field : native_fields()) {
        const auto value = prefab.raw_get<sol::object>(field.name);
        if (!is_nil(value)) {
            field.write(state, *_world, entity, value);
        }
    }
}

Entities::Record& Entities::live_record(sol::this_state state, const EntityHandle& handle) {
    const auto found = _records.find(handle.entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(handle.entity)) {
        throw std::runtime_error(std::format("{}attempt to use a destroyed entity (id {})",
                                             caller_location(state), handle.entity.id()));
    }
    return found->second;
}

} // namespace engine::script
