#include "EntityFields.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string>

#include "engine/physics/Components.hpp"
#include "entity/ScriptBody.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

bool lacks_body(const core::EntityManager& world, core::Entity entity) {
    return !world.has<physics::PhysicsBody>(entity);
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
    FieldAlias{.name = "vx",
               .component = "PhysicsBody",
               .field = "velocity",
               .shape = AliasShape::AxisX,
               .lacks_defaults = &lacks_body,
               .apply_defaults = &give_script_body},
    FieldAlias{.name = "vy",
               .component = "PhysicsBody",
               .field = "velocity",
               .shape = AliasShape::AxisY,
               .lacks_defaults = &lacks_body,
               .apply_defaults = &give_script_body},
    FieldAlias{.name = "sprite", .component = "Sprite", .field = "path"},
    FieldAlias{.name = "color", .component = "Sprite", .field = "color"},
    FieldAlias{.name = "shader", .component = "Shader", .field = "path"},
    FieldAlias{.name = "tags", .component = "Tags", .field = "labels"},
    FieldAlias{.name = "hitbox",
               .component = "AABBCollider",
               .field = "size",
               .shape = AliasShape::Size,
               .lacks_defaults = &lacks_body,
               .apply_defaults = &give_script_body,
               .after_set = &anchor_hitbox},
    FieldAlias{.name = "collides_with", .component = "CollidesWith", .field = "labels"},
    FieldAlias{.name = "zoom", .component = "Camera", .field = "zoom"},
    FieldAlias{.name = "follow", .component = "Camera", .field = "follow"},
    FieldAlias{.name = "bounds",
               .component = "Camera",
               .field = "bounds_origin",
               .paired_field = "bounds_size",
               .shape = AliasShape::Area},
    FieldAlias{.name = "rotation", .component = "Rotation", .field = "degrees"},
    FieldAlias{.name = "scale",
               .component = "Scale",
               .field = "x",
               .paired_field = "y",
               .shape = AliasShape::Factor},
};

core::FieldRef require_alias_field(const core::FieldRegistry& registry, std::string_view component,
                                   std::string_view field) {
    const core::FieldRef found = registry.find_field(component, field);
    if (!found) {
        throw core::UnknownField(component, field);
    }
    return found;
}

[[noreturn]] void fail(sol::this_state state, std::string_view message) {
    throw std::runtime_error(std::format("{}{}", caller_location(state), message));
}

core::Vec2 with_axis(core::Vec2 vector, AliasShape shape, float value) {
    (shape == AliasShape::AxisX ? vector.x : vector.y) = value;
    return vector;
}

} // namespace

EntityFields::EntityFields(const core::FieldRegistry& registry, const DeclaredAssets& textures,
                           const DeclaredAssets& shaders)
    : _registry(registry), _textures(textures), _shaders(shaders) {
    for (const FieldAlias& alias : aliases) {
        _bindings.push_back(Binding{
            .alias = alias,
            .field = require_alias_field(registry, alias.component, alias.field),
            .paired = alias.paired_field.empty()
                          ? core::FieldRef{}
                          : require_alias_field(registry, alias.component, alias.paired_field)});
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
    if (binding.alias.shape == AliasShape::Area) {
        return read_area(state, world, entity, binding);
    }
    if (binding.alias.shape == AliasShape::Factor) {
        return read_factor(state, world, entity, binding);
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
    case AliasShape::Area:
    case AliasShape::Factor:
        break;
    }
    return size_to_lua(state, vector);
}

void EntityFields::write_alias(sol::this_state state, core::EntityManager& world,
                               core::Entity entity, std::string_view name,
                               const sol::object& value) const {
    const Binding& binding = *find(name);
    if (binding.alias.shape == AliasShape::Area) {
        write_area(state, world, entity, binding, value);
        return;
    }
    if (binding.alias.shape == AliasShape::Factor) {
        write_factor(state, world, entity, binding, value);
        return;
    }
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

sol::object EntityFields::read_area(sol::this_state state, core::EntityManager& world,
                                    core::Entity entity, const Binding& binding) const {
    const auto origin = _registry.get(world, entity, binding.field);
    const auto size = _registry.get(world, entity, binding.paired);
    if (!origin || !size) {
        return sol::make_object(state, sol::lua_nil);
    }
    return area_to_lua(
        state, Area{.origin = std::get<core::Vec2>(*origin), .size = std::get<core::Vec2>(*size)});
}

void EntityFields::write_area(sol::this_state state, core::EntityManager& world,
                              core::Entity entity, const Binding& binding,
                              const sol::object& value) const {
    const std::string_view name = binding.alias.name;
    const Area area = area_from_lua(state, value, name);
    write_value(state, world, entity, binding.paired, name, area.size);
    write_value(state, world, entity, binding.field, name, area.origin);
}

sol::object EntityFields::read_factor(sol::this_state state, core::EntityManager& world,
                                      core::Entity entity, const Binding& binding) const {
    const auto x = _registry.get(world, entity, binding.field);
    const auto y = _registry.get(world, entity, binding.paired);
    if (!x || !y) {
        return sol::make_object(state, sol::lua_nil);
    }
    return to_lua(state, core::Vec2{std::get<float>(*x), std::get<float>(*y)});
}

void EntityFields::write_factor(sol::this_state state, core::EntityManager& world,
                                core::Entity entity, const Binding& binding,
                                const sol::object& value) const {
    const std::string_view name = binding.alias.name;
    const core::Vec2 factor = factor_from_lua(state, value, name);
    write_value(state, world, entity, binding.field, name, factor.x);
    write_value(state, world, entity, binding.paired, name, factor.y);
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
    require_declared_asset(state, name, field, value);
    try {
        _registry.set(world, entity, field, value);
    } catch (const core::FieldValueOutOfRange& error) {
        fail(state, std::format("entity field '{}': {}", name, error.reason()));
    }
}

void EntityFields::require_declared_asset(sol::this_state state, std::string_view name,
                                          core::FieldRef field,
                                          const core::FieldValue& value) const {
    const auto* asset = std::get_if<core::AssetName>(&value);
    if (asset == nullptr) {
        return;
    }
    const DeclaredAssets& declared =
        field.field->asset_type() == core::AssetType::Shader ? _shaders : _textures;
    if (!declared.contains(asset->path)) {
        fail(state, std::format("entity field '{}': unknown {} '{}', declare it with {}", name,
                                declared.kind(), asset->path, declared.function()));
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
    case AliasShape::Factor:
    case AliasShape::Area:
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
