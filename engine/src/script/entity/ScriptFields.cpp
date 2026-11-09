#include "ScriptFields.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "loading/Sections.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

constexpr std::string_view script_prefix = "Script.";
constexpr std::string_view script_label = "script field";

using Keys = std::vector<std::string>;

core::FieldValue to_field_value(core::ScalarValue scalar) {
    return std::visit(
        [](auto&& item) -> core::FieldValue { return std::forward<decltype(item)>(item); },
        std::move(scalar));
}

core::ScalarValue to_scalar(core::FieldValue value) {
    return std::visit(
        [](auto&& item) -> core::ScalarValue {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, core::FieldList>) {
                throw std::invalid_argument("a list is not a scalar");
            } else {
                return std::forward<decltype(item)>(item);
            }
        },
        std::move(value));
}

bool is_lua_integer(sol::this_state state, const sol::object& value) {
    lua_State* lua = state;
    value.push(lua);
    const bool integer = lua_isinteger(lua, -1) != 0;
    lua_pop(lua, 1);
    return integer;
}

std::optional<core::ScalarValue> number_of(sol::this_state state, const sol::object& value) {
    if (is_lua_integer(state, value)) {
        return core::ScalarValue{value.as<std::int64_t>()};
    }
    const auto number = static_cast<float>(value.as<double>());
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    return core::ScalarValue{number};
}

std::optional<Keys> number_keys(const sol::table& table) {
    Keys keys;
    for (const auto& [key, value] : table) {
        if (key.get_type() != sol::type::string || value.get_type() != sol::type::number) {
            return std::nullopt;
        }
        keys.push_back(key.as<std::string>());
    }
    std::ranges::sort(keys);
    return keys;
}

std::optional<core::FieldKind> record_kind(const Keys& keys) {
    if (keys == Keys{"x", "y"}) {
        return core::FieldKind::Vec2;
    }
    if (keys == Keys{"b", "g", "r"} || keys == Keys{"a", "b", "g", "r"}) {
        return core::FieldKind::Color;
    }
    return std::nullopt;
}

std::optional<core::ScalarValue> record_of(sol::this_state state, const sol::table& table) {
    const auto keys = number_keys(table);
    const auto kind = keys ? record_kind(*keys) : std::nullopt;
    if (!kind) {
        return std::nullopt;
    }
    try {
        return to_scalar(from_lua(state, table, *kind, script_label, "default"));
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

std::optional<core::ScalarValue> scalar_of(sol::this_state state, const sol::object& value) {
    switch (value.get_type()) {
    case sol::type::number:
        return number_of(state, value);
    case sol::type::boolean:
        return core::ScalarValue{value.as<bool>()};
    case sol::type::string:
        return core::ScalarValue{value.as<std::string>()};
    case sol::type::table:
        return record_of(state, value.as<sol::table>());
    default:
        return std::nullopt;
    }
}

bool is_number_kind(const core::ScalarValue& item) {
    const core::FieldKind kind = core::kind_of(item);
    return kind == core::FieldKind::Int || kind == core::FieldKind::Float;
}

void widen_to_floats(std::vector<core::ScalarValue>& items) {
    for (core::ScalarValue& item : items) {
        if (const auto* integer = std::get_if<std::int64_t>(&item)) {
            item = static_cast<float>(*integer);
        }
    }
}

std::optional<core::FieldList> unified_list(std::vector<core::ScalarValue> items) {
    const bool numbers = std::ranges::all_of(items, is_number_kind);
    if (numbers && std::ranges::any_of(items, [](const core::ScalarValue& item) {
            return core::kind_of(item) == core::FieldKind::Float;
        })) {
        widen_to_floats(items);
    }
    const core::FieldKind element = core::kind_of(items.front());
    if (!std::ranges::all_of(items, [element](const core::ScalarValue& item) {
            return core::kind_of(item) == element;
        })) {
        return std::nullopt;
    }
    return core::FieldList{.element = element, .items = std::move(items)};
}

std::size_t entry_count(const sol::table& table) {
    std::size_t count = 0;
    for ([[maybe_unused]] const auto& entry : table) {
        ++count;
    }
    return count;
}

std::optional<core::FieldList> list_of(sol::this_state state, const sol::table& table) {
    const std::size_t size = table.size();
    if (size == 0 || entry_count(table) != size) {
        return std::nullopt;
    }
    std::vector<core::ScalarValue> items;
    items.reserve(size);
    for (std::size_t index = 1; index <= size; ++index) {
        auto item = scalar_of(state, table.raw_get<sol::object>(index));
        if (!item) {
            return std::nullopt;
        }
        items.push_back(std::move(*item));
    }
    return unified_list(std::move(items));
}

std::optional<core::FieldValue> default_of(sol::this_state state, const sol::object& value) {
    if (value.get_type() == sol::type::table) {
        const auto table = value.as<sol::table>();
        if (auto record = record_of(state, table)) {
            return to_field_value(std::move(*record));
        }
        if (auto list = list_of(state, table)) {
            return core::FieldValue{std::move(*list)};
        }
        return std::nullopt;
    }
    if (auto scalar = scalar_of(state, value)) {
        return to_field_value(std::move(*scalar));
    }
    return std::nullopt;
}

bool is_section(std::string_view name) {
    return std::ranges::find(section_names, name) != section_names.end();
}

bool is_free_field_name(const sol::object& key, const Entities& entities) {
    if (key.get_type() != sol::type::string) {
        return false;
    }
    const auto name = key.as<std::string_view>();
    return !name.empty() && !name.starts_with('_') && !is_section(name) &&
           name != persistent_field && !entities.is_native_field(name);
}

} // namespace

std::vector<ScriptField> infer_script_fields(sol::this_state state, const sol::table& prefab,
                                             const Entities& entities) {
    std::vector<ScriptField> fields;
    for (const auto& [key, value] : prefab) {
        if (!is_free_field_name(key, entities)) {
            continue;
        }
        if (auto default_value = default_of(state, value)) {
            fields.push_back({.name = key.as<std::string>(), .default_value = *default_value});
        }
    }
    std::ranges::sort(fields, {}, &ScriptField::name);
    return fields;
}

std::optional<std::string_view> script_field_name(std::string_view key) noexcept {
    if (!key.starts_with(script_prefix)) {
        return std::nullopt;
    }
    return key.substr(script_prefix.size());
}

std::string script_field_key(std::string_view name) {
    return std::string(script_prefix) + std::string(name);
}

const ScriptField* find_script_field(const Declaration& declaration,
                                     std::string_view name) noexcept {
    const auto found = std::ranges::find(declaration.script_fields, name, &ScriptField::name);
    return found == declaration.script_fields.end() ? nullptr : &*found;
}

core::FieldValue script_value_from_lua(sol::this_state state, const sol::object& value,
                                       const ScriptField& field) {
    return from_lua(state, value, field.kind(), field.element(), script_label, field.name);
}

bool has_shape_of(const core::FieldValue& value, const ScriptField& field) noexcept {
    if (core::kind_of(value) != field.kind()) {
        return false;
    }
    const auto* list = std::get_if<core::FieldList>(&value);
    return list == nullptr || list->element == field.element();
}

} // namespace engine::script
