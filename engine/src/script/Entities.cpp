#include "Entities.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "Declarations.hpp"
#include "NativeFields.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::array<std::string_view, 4> read_only_fields{"id", "type", "age", "input"};

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

float input_axis(const InputState& input, std::string_view name) {
    const auto found = input.axes.find(name);
    return found == input.axes.end() ? 0.F : found->second;
}

bool input_pressed(const InputState& input, std::string_view name) {
    return input.pressed.contains(name);
}

} // namespace

Entities::Entities(sol::state& lua, const Players& players)
    : _lua(lua), _players(players), _methods(lua.create_table()) {
    sol::table types = _lua.create_table();
    types.new_usertype<EntityHandle>(
        "EntityHandle", sol::no_constructor, sol::meta_function::index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key) {
            return get(state, handle, key);
        },
        sol::meta_function::new_index,
        [this](sol::this_state state, const EntityHandle& handle, const sol::object& key,
               const sol::object& value) { set(state, handle, key, value); });
    types.new_usertype<InputState>("InputState", sol::no_constructor, "axis", &input_axis,
                                   "pressed", &input_pressed);
    _methods.set_function("destroy", [this](sol::this_state state, const EntityHandle& handle) {
        destroy(state, handle);
    });
    _methods.set_function("has_tag",
                          [this](sol::this_state state, const EntityHandle& handle,
                                 std::string_view label) { return has_tag(state, handle, label); });
}

void Entities::attach(core::EntityManager& world) {
    register_if_missing<core::Position>(world);
    register_if_missing<core::Velocity>(world);
    register_if_missing<core::Sprite>(world);
    register_if_missing<core::Tags>(world);
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
                                     .handle = sol::make_object(_lua, EntityHandle{entity}),
                                     .input = {}});
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

void Entities::release(core::Entity entity) { _records.erase(entity.id()); }

bool Entities::call(core::Entity entity, std::string_view method) {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || _world == nullptr || !_world->alive(entity)) {
        log::error("lua: cannot call '{}' on destroyed entity (id {})", method, entity.id());
        return false;
    }
    return invoke(entity, method);
}

void Entities::set_input(core::Entity entity, InputState input) {
    const auto found = _records.find(entity.id());
    if (found != _records.end()) {
        found->second.input = std::move(input);
    }
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
        if (name == "input") {
            return sol::make_object(state, input_of(record));
        }
        if (name == "owner") {
            return record.owner ? player_object(state, *record.owner)
                                : sol::make_object(state, sol::lua_nil);
        }
        if (const auto field = find_native_field(name)) {
            return field->read(state, *_world, handle.entity);
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
        if (name == "owner") {
            set_owner(state, record, value);
            return;
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

std::vector<core::Entity> Entities::owned_by(std::uint32_t player_id) const {
    std::vector<core::Entity> owned;
    for (const core::Entity entity : live_entities()) {
        const Record& record = _records.at(entity.id());
        if (!record.doomed && record.owner == player_id) {
            owned.push_back(entity);
        }
    }
    return owned;
}

std::optional<std::string> Entities::type_of(core::Entity entity) const {
    const auto found = _records.find(entity.id());
    if (found == _records.end()) {
        return std::nullopt;
    }
    return found->second.type;
}

std::optional<std::uint32_t> Entities::owner_of(core::Entity entity) const {
    const auto found = _records.find(entity.id());
    if (found == _records.end() || !found->second.owner ||
        !_players.contains(*found->second.owner)) {
        return std::nullopt;
    }
    return found->second.owner;
}

sol::object Entities::handle_of(core::Entity entity) const {
    return _records.at(entity.id()).handle;
}

sol::object Entities::player_object(sol::this_state state, std::uint32_t player_id) const {
    if (!_players.contains(player_id)) {
        return sol::make_object(state, sol::lua_nil);
    }
    return sol::make_object(state, PlayerHandle{player_id});
}

const InputState& Entities::input_of(const Record& record) const {
    if (record.owner) {
        if (const InputState* owned = _players.input(*record.owner)) {
            return *owned;
        }
    }
    return record.input;
}

void Entities::set_owner(sol::this_state state, Record& record, const sol::object& value) {
    if (is_nil(value)) {
        record.owner.reset();
        return;
    }
    if (!value.is<PlayerHandle>()) {
        throw std::runtime_error(
            std::format("{}entity field 'owner' must be a player or nil", caller_location(state)));
    }
    const std::uint32_t player_id = value.as<PlayerHandle>().id;
    if (!_players.contains(player_id)) {
        throw std::runtime_error(
            std::format("{}player {} is not connected", caller_location(state), player_id));
    }
    record.owner = player_id;
}

bool Entities::has_tag(sol::this_state state, const EntityHandle& handle, std::string_view label) {
    live_record(state, handle);
    return _world->has<core::Tags>(handle.entity) &&
           _world->get<core::Tags>(handle.entity).has(label);
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
