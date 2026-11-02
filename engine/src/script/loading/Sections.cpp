#include "Sections.hpp"

#include <format>
#include <optional>
#include <string>

#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

bool is_nil(const sol::object& value) {
    return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
}

std::string describe_key(const sol::object& key) {
    if (key.get_type() == sol::type::string) {
        return std::format("'{}'", key.as<std::string_view>());
    }
    return std::format("a key of type {}", sol::type_name(key.lua_state(), key.get_type()));
}

std::string label(const Declaration& declaration) {
    return std::format("{}: {} '{}'", declaration.source.generic_string(),
                       kind_name(declaration.kind), declaration.name);
}

std::optional<sol::table> section_table(const sol::table& target, const std::string& label,
                                        std::string_view name, bool& valid) {
    const auto value = target.raw_get<sol::object>(name);
    if (is_nil(value)) {
        return std::nullopt;
    }
    if (value.get_type() != sol::type::table) {
        log::error("lua: {}: section '{}' must be a table", label, name);
        valid = false;
        return std::nullopt;
    }
    return value.as<sol::table>();
}

bool has_no_conflict(const sol::table& target, const std::string& label, std::string_view name,
                     const sol::table& section) {
    for (const auto& [key, value] : section) {
        if (!is_nil(target.raw_get<sol::object>(key))) {
            log::error("lua: {} sets {} both outside and inside section '{}'", label,
                       describe_key(key), name);
            return false;
        }
    }
    return true;
}

} // namespace

bool apply_section(const Declaration& declaration, std::string_view section) {
    return apply_section(declaration.prefab, label(declaration), section);
}

bool apply_section(sol::table target, const std::string& label, std::string_view section) {
    bool valid = true;
    std::optional<sol::table> kept;
    for (const std::string_view name : section_names) {
        const auto table = section_table(target, label, name, valid);
        if (!table) {
            continue;
        }
        valid = has_no_conflict(target, label, name, *table) && valid;
        if (name == section) {
            kept = table;
        }
    }
    if (!valid) {
        return false;
    }
    for (const std::string_view name : section_names) {
        target.raw_set(name, sol::lua_nil);
    }
    if (kept) {
        for (const auto& [key, value] : *kept) {
            target.raw_set(key, value);
        }
    }
    return true;
}

} // namespace engine::script
