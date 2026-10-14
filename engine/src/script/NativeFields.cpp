#include "NativeFields.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <stdexcept>
#include <type_traits>

#include "Declarations.hpp"
#include "engine/core/ComponentPhysics.hpp"

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

constexpr std::array fields{
    member_field<core::Position, [](core::Position& c) -> float& { return c.pos.x; }>("x"),
    member_field<core::Position, [](core::Position& c) -> float& { return c.pos.y; }>("y"),
    member_field<core::Velocity, [](core::Velocity& c) -> float& { return c.vel.x; }>("vx"),
    member_field<core::Velocity, [](core::Velocity& c) -> float& { return c.vel.y; }>("vy"),
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
