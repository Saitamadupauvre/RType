#include "LuaFieldValue.hpp"

#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "Declarations.hpp"

namespace engine::script {

namespace {

using core::FieldKind;

[[noreturn]] void fail(sol::this_state state, std::string_view message) {
    throw std::runtime_error(std::format("{}{}", caller_location(state), message));
}

struct ValueName {
    std::string_view label;
    std::string name;
};

std::string describe(const ValueName& name) {
    return std::format("{} '{}'", name.label, name.name);
}

ValueName entity_field(std::string_view name) {
    return ValueName{.label = "entity field", .name = std::string(name)};
}

void require_type(sol::this_state state, const sol::object& value, sol::type expected,
                  const ValueName& name) {
    if (value.get_type() != expected) {
        fail(state, std::format("{} cannot be set to a {}", describe(name),
                                sol::type_name(state, value.get_type())));
    }
}

float finite_float(sol::this_state state, double number, const ValueName& name) {
    const auto narrowed = static_cast<float>(number);
    if (!std::isfinite(narrowed)) {
        fail(state, std::format("{} must be a finite number", describe(name)));
    }
    return narrowed;
}

std::int64_t whole_number(sol::this_state state, const sol::object& value, const ValueName& name) {
    lua_State* lua = state;
    value.push(lua);
    int converted = 0;
    const lua_Integer number = lua_tointegerx(lua, -1, &converted);
    lua_pop(lua, 1);
    if (converted == 0) {
        fail(state, std::format("{} must be a whole number", describe(name)));
    }
    return number;
}

double table_number(sol::this_state state, const sol::table& table, std::string_view key,
                    const ValueName& name) {
    const auto value = table.raw_get<sol::object>(key);
    if (value.get_type() != sol::type::number) {
        fail(state, std::format("{} needs a number '{}'", describe(name), key));
    }
    return value.as<double>();
}

core::Vec2 vec2_from_lua(sol::this_state state, const sol::object& value, const ValueName& name) {
    require_type(state, value, sol::type::table, name);
    const auto table = value.as<sol::table>();
    return core::Vec2{finite_float(state, table_number(state, table, "x", name), name),
                      finite_float(state, table_number(state, table, "y", name), name)};
}

std::uint8_t color_channel(sol::this_state state, const sol::table& table, std::string_view key,
                           std::optional<std::uint8_t> fallback, const ValueName& name) {
    if (fallback && table.raw_get<sol::object>(key).get_type() == sol::type::lua_nil) {
        return *fallback;
    }
    const double channel = table_number(state, table, key, name);
    if (!(channel >= 0.0 && channel <= 255.0)) {
        fail(state, std::format("{}: '{}' must be in [0, 255]", describe(name), key));
    }
    return static_cast<std::uint8_t>(channel);
}

core::Rgba color_from_lua(sol::this_state state, const sol::object& value, const ValueName& name) {
    require_type(state, value, sol::type::table, name);
    const auto table = value.as<sol::table>();
    return core::Rgba{.r = color_channel(state, table, "r", std::nullopt, name),
                      .g = color_channel(state, table, "g", std::nullopt, name),
                      .b = color_channel(state, table, "b", std::nullopt, name),
                      .a = color_channel(state, table, "a", 255, name)};
}

sol::type lua_type_of(FieldKind kind) {
    switch (kind) {
    case FieldKind::Float:
    case FieldKind::Int:
        return sol::type::number;
    case FieldKind::Bool:
        return sol::type::boolean;
    case FieldKind::String:
    case FieldKind::Asset:
        return sol::type::string;
    case FieldKind::Vec2:
    case FieldKind::Color:
    case FieldKind::List:
        return sol::type::table;
    }
    return sol::type::none;
}

core::ScalarValue scalar_from_lua(sol::this_state state, const sol::object& value, FieldKind kind,
                                  const ValueName& name) {
    require_type(state, value, lua_type_of(kind), name);
    switch (kind) {
    case FieldKind::Float:
        return finite_float(state, value.as<double>(), name);
    case FieldKind::Int:
        return whole_number(state, value, name);
    case FieldKind::Bool:
        return value.as<bool>();
    case FieldKind::String:
        return value.as<std::string>();
    case FieldKind::Asset:
        return core::AssetName{value.as<std::string>()};
    case FieldKind::Vec2:
        return vec2_from_lua(state, value, name);
    case FieldKind::Color:
        return color_from_lua(state, value, name);
    case FieldKind::List:
        break;
    }
    fail(state, std::format("{} cannot hold a nested list", describe(name)));
}

core::FieldList list_from_lua(sol::this_state state, const sol::object& value, FieldKind element,
                              const ValueName& name) {
    require_type(state, value, sol::type::table, name);
    const auto table = value.as<sol::table>();
    core::FieldList list{.element = element, .items = {}};
    for (std::size_t index = 1; index <= table.size(); ++index) {
        const auto item = table.raw_get<sol::object>(index);
        if (item.get_type() != lua_type_of(element)) {
            fail(state, std::format("{} must only hold {}s", describe(name),
                                    core::field_kind_name(element)));
        }
        list.items.push_back(scalar_from_lua(
            state, item, element,
            ValueName{.label = name.label, .name = std::format("{}[{}]", name.name, index)}));
    }
    return list;
}

sol::object scalar_to_lua(sol::state_view lua, const core::ScalarValue& value) {
    return std::visit(
        [&lua]<typename Value>(const Value& held) -> sol::object {
            if constexpr (std::same_as<Value, core::Vec2>) {
                return lua.create_table_with("x", held.x, "y", held.y);
            } else if constexpr (std::same_as<Value, core::Rgba>) {
                return lua.create_table_with("r", held.r, "g", held.g, "b", held.b, "a", held.a);
            } else if constexpr (std::same_as<Value, core::AssetName>) {
                return held.path.empty() ? sol::make_object(lua, sol::lua_nil)
                                         : sol::make_object(lua, held.path);
            } else {
                return sol::make_object(lua, held);
            }
        },
        value);
}

} // namespace

sol::object to_lua(sol::this_state state, const core::FieldValue& value) {
    sol::state_view lua(state);
    if (const auto* list = std::get_if<core::FieldList>(&value)) {
        sol::table items = lua.create_table(static_cast<int>(list->items.size()), 0);
        for (const core::ScalarValue& item : list->items) {
            items.add(scalar_to_lua(lua, item));
        }
        return items;
    }
    return std::visit(
        [&lua]<typename Value>(const Value& held) -> sol::object {
            if constexpr (std::same_as<Value, core::FieldList>) {
                return sol::make_object(lua, sol::lua_nil);
            } else {
                return scalar_to_lua(lua, core::ScalarValue{held});
            }
        },
        value);
}

core::FieldValue from_lua(sol::this_state state, const sol::object& value,
                          const core::FieldDescriptor& field, std::string_view name) {
    if (field.kind() == FieldKind::List) {
        return list_from_lua(state, value, field.element_kind().value_or(FieldKind::String),
                             entity_field(name));
    }
    return from_lua(state, value, field.kind(), "entity field", name);
}

core::FieldValue from_lua(sol::this_state state, const sol::object& value, FieldKind kind,
                          std::string_view label, std::string_view name) {
    return std::visit(
        []<typename Value>(Value&& held) { return core::FieldValue{std::forward<Value>(held)}; },
        scalar_from_lua(state, value, kind, ValueName{.label = label, .name = std::string(name)}));
}

float number_from_lua(sol::this_state state, const sol::object& value, std::string_view name) {
    const ValueName field = entity_field(name);
    require_type(state, value, sol::type::number, field);
    return finite_float(state, value.as<double>(), field);
}

sol::object size_to_lua(sol::this_state state, core::Vec2 size) {
    return sol::state_view(state).create_table_with("w", size.x, "h", size.y);
}

core::Vec2 size_from_lua(sol::this_state state, const sol::object& value, std::string_view name) {
    const ValueName field = entity_field(name);
    require_type(state, value, sol::type::table, field);
    const auto table = value.as<sol::table>();
    return core::Vec2{finite_float(state, table_number(state, table, "w", field), field),
                      finite_float(state, table_number(state, table, "h", field), field)};
}

} // namespace engine::script
