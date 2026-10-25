#include "NativeFields.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "Declarations.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/physics/Components.hpp"

namespace engine::script {

namespace {

template <typename Access, typename Component>
concept FieldAccessor = std::invocable<Access, Component&> &&
                        std::is_lvalue_reference_v<std::invoke_result_t<Access, Component&>>;

template <typename Value> bool lua_type_matches(const sol::object& value) {
    if constexpr (std::is_arithmetic_v<Value>) {
        return value.get_type() == sol::type::number;
    } else {
        return value.is<Value>();
    }
}

template <typename Component, FieldAccessor<Component> auto Access>
sol::object read_member(sol::this_state state, core::EntityManager& world, core::Entity entity) {
    if (!world.has<Component>(entity)) {
        return sol::make_object(state, sol::lua_nil);
    }
    return sol::make_object(state, Access(world.get<Component>(entity)));
}

template <typename Component, FieldAccessor<Component> auto Access>
void write_member(sol::this_state state, core::EntityManager& world, core::Entity entity,
                  const sol::object& value) {
    using Value = std::remove_cvref_t<std::invoke_result_t<decltype(Access), Component&>>;
    if (!lua_type_matches<Value>(value)) {
        throw std::runtime_error(std::format("{}entity field cannot be set to a {}",
                                             caller_location(state),
                                             sol::type_name(state, value.get_type())));
    }
    if (!world.has<Component>(entity)) {
        world.add(entity, Component{});
    }
    Access(world.get<Component>(entity)) = value.as<Value>();
}

template <typename Component, FieldAccessor<Component> auto Access>
constexpr NativeField member_field(std::string_view name) {
    return NativeField{.name = name,
                       .read = &read_member<Component, Access>,
                       .write = &write_member<Component, Access>};
}

core::Sprite& sprite_of(core::EntityManager& world, core::Entity entity) {
    if (!world.has<core::Sprite>(entity)) {
        world.add(entity, core::Sprite{});
    }
    return world.get<core::Sprite>(entity);
}

void require_type(sol::this_state state, std::string_view field, const sol::object& value,
                  sol::type expected) {
    if (value.get_type() != expected) {
        throw std::runtime_error(std::format("{}entity field '{}' cannot be set to a {}",
                                             caller_location(state), field,
                                             sol::type_name(state, value.get_type())));
    }
}

sol::object read_sprite(sol::this_state state, core::EntityManager& world, core::Entity entity) {
    if (!world.has<core::Sprite>(entity) || world.get<core::Sprite>(entity).path.empty()) {
        return sol::make_object(state, sol::lua_nil);
    }
    return sol::make_object(state, world.get<core::Sprite>(entity).path);
}

void write_sprite(sol::this_state state, core::EntityManager& world, core::Entity entity,
                  const sol::object& value) {
    require_type(state, "sprite", value, sol::type::string);
    sprite_of(world, entity).path = value.as<std::string>();
}

sol::object read_color(sol::this_state state, core::EntityManager& world, core::Entity entity) {
    if (!world.has<core::Sprite>(entity)) {
        return sol::make_object(state, sol::lua_nil);
    }
    const core::Rgba& color = world.get<core::Sprite>(entity).color;
    sol::state_view lua(state);
    return lua.create_table_with("r", color.r, "g", color.g, "b", color.b, "a", color.a);
}

std::uint8_t color_channel(sol::this_state state, const sol::table& table, std::string_view name,
                           std::optional<std::uint8_t> fallback) {
    const auto value = table.raw_get<sol::object>(name);
    if (value.get_type() != sol::type::number) {
        if (fallback && (value.get_type() == sol::type::lua_nil)) {
            return *fallback;
        }
        throw std::runtime_error(std::format("{}entity field 'color' needs a number '{}'",
                                             caller_location(state), name));
    }
    const double channel = value.as<double>();
    if (channel < 0.0 || channel > 255.0) {
        throw std::runtime_error(std::format("{}entity field 'color': '{}' must be in [0, 255]",
                                             caller_location(state), name));
    }
    return static_cast<std::uint8_t>(channel);
}

void write_color(sol::this_state state, core::EntityManager& world, core::Entity entity,
                 const sol::object& value) {
    require_type(state, "color", value, sol::type::table);
    const auto table = value.as<sol::table>();
    const core::Rgba color{.r = color_channel(state, table, "r", std::nullopt),
                           .g = color_channel(state, table, "g", std::nullopt),
                           .b = color_channel(state, table, "b", std::nullopt),
                           .a = color_channel(state, table, "a", 255)};
    sprite_of(world, entity).color = color;
}

sol::object read_tags(sol::this_state state, core::EntityManager& world, core::Entity entity) {
    sol::state_view lua(state);
    sol::table labels = lua.create_table();
    if (world.has<core::Tags>(entity)) {
        for (const std::string& label : world.get<core::Tags>(entity).labels()) {
            labels.add(label);
        }
    }
    return labels;
}

core::Tags tags_from_table(sol::this_state state, std::string_view field, const sol::table& table) {
    core::Tags tags;
    for (std::size_t index = 1; index <= table.size(); ++index) {
        const auto label = table.raw_get<sol::object>(index);
        if (label.get_type() != sol::type::string) {
            throw std::runtime_error(std::format("{}entity field '{}' must only hold strings",
                                                 caller_location(state), field));
        }
        tags.add(label.as<std::string>());
    }
    return tags;
}

void write_tags(sol::this_state state, core::EntityManager& world, core::Entity entity,
                const sol::object& value) {
    require_type(state, "tags", value, sol::type::table);
    world.add(entity, tags_from_table(state, "tags", value.as<sol::table>()));
}

sol::object read_collides_with(sol::this_state state, core::EntityManager& world,
                               core::Entity entity) {
    sol::state_view lua(state);
    sol::table labels = lua.create_table();
    if (world.has<CollisionFilter>(entity)) {
        for (const std::string& label : world.get<CollisionFilter>(entity).tags.labels()) {
            labels.add(label);
        }
    }
    return labels;
}

void write_collides_with(sol::this_state state, core::EntityManager& world, core::Entity entity,
                         const sol::object& value) {
    require_type(state, "collides_with", value, sol::type::table);
    world.add(entity, CollisionFilter{
                          .tags = tags_from_table(state, "collides_with", value.as<sol::table>())});
}

sol::object read_hitbox(sol::this_state state, core::EntityManager& world, core::Entity entity) {
    if (!world.has<physics::AABBCollider>(entity)) {
        return sol::make_object(state, sol::lua_nil);
    }
    const core::Vec2 size = world.get<physics::AABBCollider>(entity).size;
    sol::state_view lua(state);
    return lua.create_table_with("w", size.x, "h", size.y);
}

float hitbox_side(sol::this_state state, const sol::table& table, std::string_view name) {
    const auto value = table.raw_get<sol::object>(name);
    if (value.get_type() != sol::type::number) {
        throw std::runtime_error(std::format("{}entity field 'hitbox' needs a number '{}'",
                                             caller_location(state), name));
    }
    const double side = value.as<double>();
    if (!std::isfinite(side) || side <= 0.0) {
        throw std::runtime_error(std::format("{}entity field 'hitbox': '{}' must be positive",
                                             caller_location(state), name));
    }
    return static_cast<float>(side);
}

void write_hitbox(sol::this_state state, core::EntityManager& world, core::Entity entity,
                  const sol::object& value) {
    require_type(state, "hitbox", value, sol::type::table);
    const auto table = value.as<sol::table>();
    const core::Vec2 size{hitbox_side(state, table, "w"), hitbox_side(state, table, "h")};
    world.add(entity, physics::AABBCollider{.offset = size / 2.0F, .size = size});
    if (!world.has<physics::PhysicsBody>(entity)) {
        world.add(entity, physics::PhysicsBody{.type = physics::PhysicsBody::Type::Dynamic,
                                               .resolve_collisions = false,
                                               .affected_by_gravity = false});
    }
    if (!world.has<physics::Scriptable>(entity)) {
        world.add(entity, physics::Scriptable{});
    }
}

constexpr std::array fields{
    member_field<core::Position, [](core::Position& c) -> float& { return c.pos.x; }>("x"),
    member_field<core::Position, [](core::Position& c) -> float& { return c.pos.y; }>("y"),
    member_field<core::Velocity, [](core::Velocity& c) -> float& { return c.vel.x; }>("vx"),
    member_field<core::Velocity, [](core::Velocity& c) -> float& { return c.vel.y; }>("vy"),
    NativeField{.name = "sprite", .read = &read_sprite, .write = &write_sprite},
    NativeField{.name = "color", .read = &read_color, .write = &write_color},
    NativeField{.name = "tags", .read = &read_tags, .write = &write_tags},
    NativeField{.name = "hitbox", .read = &read_hitbox, .write = &write_hitbox},
    NativeField{
        .name = "collides_with", .read = &read_collides_with, .write = &write_collides_with},
};

} // namespace

std::span<const NativeField> native_fields() { return fields; }

std::optional<NativeField> find_native_field(std::string_view name) {
    const auto found = std::ranges::find(fields, name, &NativeField::name);
    if (found == fields.end()) {
        return std::nullopt;
    }
    return *found;
}

} // namespace engine::script
