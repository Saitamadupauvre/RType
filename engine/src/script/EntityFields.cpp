#include "EntityFields.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string>

#include "Declarations.hpp"
#include "LuaFieldValue.hpp"
#include "engine/physics/Components.hpp"

namespace engine::script {

namespace {

bool lacks_body(const core::EntityManager& world, core::Entity entity) {
    return !world.has<physics::PhysicsBody>(entity);
}

void give_script_body(core::EntityManager& world, core::Entity entity) {
    world.add(entity, physics::PhysicsBody{.type = physics::PhysicsBody::Type::Dynamic,
                                           .resolve_collisions = false,
                                           .affected_by_gravity = false});
}

void anchor_hitbox(core::EntityManager& world, core::Entity entity) {
    auto& collider = world.get<physics::AABBCollider>(entity);
    collider.offset = collider.size / 2.0F;
    if (!world.has<physics::Scriptable>(entity)) {
        world.add(entity, physics::Scriptable{});
    }
}

constexpr std::array aliases{
    FieldAlias{.name = "x", .component = "Position", .field = "pos", .shape = AliasShape::AxisX},
    FieldAlias{.name = "y", .component = "Position", .field = "pos", .shape = AliasShape::AxisY},
    FieldAlias{.name = "vx", .component = "Velocity", .field = "vel", .shape = AliasShape::AxisX},
    FieldAlias{.name = "vy", .component = "Velocity", .field = "vel", .shape = AliasShape::AxisY},
    FieldAlias{.name = "sprite", .component = "Sprite", .field = "path"},
    FieldAlias{.name = "color", .component = "Sprite", .field = "color"},
    FieldAlias{.name = "tags", .component = "Tags", .field = "labels"},
    FieldAlias{.name = "hitbox",
               .component = "AABBCollider",
               .field = "size",
               .shape = AliasShape::Size,
               .lacks_defaults = &lacks_body,
               .apply_defaults = &give_script_body,
               .after_set = &anchor_hitbox},
    FieldAlias{.name = "collides_with", .component = "CollidesWith", .field = "labels"},
};

[[noreturn]] void fail(sol::this_state state, std::string_view message) {
    throw std::runtime_error(std::format("{}{}", caller_location(state), message));
}

core::Vec2 with_axis(core::Vec2 vector, AliasShape shape, float value) {
    (shape == AliasShape::AxisX ? vector.x : vector.y) = value;
    return vector;
}

} // namespace

EntityFields::EntityFields(const core::FieldRegistry& registry) : _registry(registry) {
    for (const FieldAlias& alias : aliases) {
        const core::FieldRef field = registry.find_field(alias.component, alias.field);
        if (!field) {
            throw core::UnknownField(alias.component, alias.field);
        }
        _bindings.push_back(Binding{.alias = alias, .field = field});
    }
}

bool EntityFields::has_alias(std::string_view name) const { return find(name) != nullptr; }

std::vector<std::string_view> EntityFields::alias_names() const {
    std::vector<std::string_view> names;
    names.reserve(_bindings.size());
    for (const Binding& binding : _bindings) {
        names.push_back(binding.alias.name);
    }
    return names;
}

sol::object EntityFields::read_alias(sol::this_state state, core::EntityManager& world,
                                     core::Entity entity, std::string_view name) const {
    const Binding& binding = *find(name);
    if (binding.alias.shape == AliasShape::Whole) {
        return read_field(state, world, entity, binding.field);
    }
    const auto value = _registry.get(world, entity, binding.field);
    if (!value) {
        return sol::make_object(state, sol::lua_nil);
    }
    const auto vector = std::get<core::Vec2>(*value);
    switch (binding.alias.shape) {
    case AliasShape::AxisX:
        return sol::make_object(state, vector.x);
    case AliasShape::AxisY:
        return sol::make_object(state, vector.y);
    case AliasShape::Size:
    case AliasShape::Whole:
        break;
    }
    return size_to_lua(state, vector);
}

void EntityFields::write_alias(sol::this_state state, core::EntityManager& world,
                               core::Entity entity, std::string_view name,
                               const sol::object& value) const {
    const Binding& binding = *find(name);
    const core::FieldValue converted = alias_value(state, world, entity, binding, value);
    const bool lacked_defaults =
        binding.alias.lacks_defaults != nullptr && binding.alias.lacks_defaults(world, entity);
    write_value(state, world, entity, binding.field, name, converted);
    if (lacked_defaults) {
        binding.alias.apply_defaults(world, entity);
    }
    if (binding.alias.after_set != nullptr) {
        binding.alias.after_set(world, entity);
    }
}

sol::object EntityFields::read(sol::this_state state, core::EntityManager& world,
                               core::Entity entity, std::string_view component,
                               std::string_view field) const {
    return read_field(state, world, entity, require(state, component, field));
}

void EntityFields::write(sol::this_state state, core::EntityManager& world, core::Entity entity,
                         std::string_view component, std::string_view field,
                         const sol::object& value) const {
    const core::FieldRef found = require(state, component, field);
    const std::string name = std::format("{}.{}", component, field);
    write_value(state, world, entity, found, name, from_lua(state, value, *found.field, name));
}

const EntityFields::Binding* EntityFields::find(std::string_view name) const {
    const auto found = std::ranges::find(_bindings, name,
                                         [](const Binding& binding) { return binding.alias.name; });
    return found == _bindings.end() ? nullptr : &*found;
}

core::FieldRef EntityFields::require(sol::this_state state, std::string_view component,
                                     std::string_view field) const {
    const core::FieldRef found = _registry.find_field(component, field);
    if (!found) {
        fail(state, std::format("unknown entity field '{}.{}'", component, field));
    }
    return found;
}

sol::object EntityFields::read_field(sol::this_state state, core::EntityManager& world,
                                     core::Entity entity, core::FieldRef field) const {
    const auto value = _registry.get(world, entity, field);
    if (value) {
        return to_lua(state, *value);
    }
    if (field.field->kind() == core::FieldKind::List) {
        return sol::state_view(state).create_table();
    }
    return sol::make_object(state, sol::lua_nil);
}

void EntityFields::write_value(sol::this_state state, core::EntityManager& world,
                               core::Entity entity, core::FieldRef field, std::string_view name,
                               const core::FieldValue& value) const {
    try {
        _registry.set(world, entity, field, value);
    } catch (const core::FieldValueOutOfRange& error) {
        fail(state, std::format("entity field '{}': {}", name, error.reason()));
    }
}

core::FieldValue EntityFields::alias_value(sol::this_state state, core::EntityManager& world,
                                           core::Entity entity, const Binding& binding,
                                           const sol::object& value) const {
    const std::string_view name = binding.alias.name;
    switch (binding.alias.shape) {
    case AliasShape::Whole:
        return from_lua(state, value, *binding.field.field, name);
    case AliasShape::Size:
        return size_from_lua(state, value, name);
    case AliasShape::AxisX:
    case AliasShape::AxisY:
        break;
    }
    const float number = number_from_lua(state, value, name);
    const auto current = _registry.get(world, entity, binding.field);
    const core::Vec2 vector = current ? std::get<core::Vec2>(*current) : core::Vec2{};
    return with_axis(vector, binding.alias.shape, number);
}

} // namespace engine::script
