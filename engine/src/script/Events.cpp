#include "Events.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <variant>

#include "Declarations.hpp"
#include "LuaFieldValue.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

[[noreturn]] void fail(sol::this_state state, std::string_view message) {
    throw std::runtime_error(std::format("{}{}", caller_location(state), message));
}

bool is_letter(char character) noexcept {
    return std::isalpha(static_cast<unsigned char>(character)) != 0;
}

bool is_identifier_character(char character) noexcept {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool is_name_character(char character) noexcept {
    return is_identifier_character(character) || character == '-' || character == '.';
}

bool is_valid_event_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= max_event_name_length && is_letter(name.front()) &&
           std::ranges::all_of(name, is_name_character);
}

bool is_valid_key(std::string_view key) noexcept {
    return !key.empty() && key.size() <= max_event_name_length &&
           (is_letter(key.front()) || key.front() == '_') &&
           std::ranges::all_of(key, is_identifier_character);
}

std::optional<core::FieldKind> event_kind(std::string_view name) noexcept {
    const auto found = std::ranges::find(core::field_kinds, name, &core::FieldKindInfo::name);
    if (found == core::field_kinds.end() || found->kind == core::FieldKind::List) {
        return std::nullopt;
    }
    return found->kind;
}

std::string event_kind_names() {
    std::string names;
    for (const core::FieldKindInfo& info : core::field_kinds) {
        if (info.kind != core::FieldKind::List) {
            names += names.empty() ? "" : ", ";
            names += info.name;
        }
    }
    return names;
}

std::string checked_name(sol::this_state state, std::string_view function,
                         const sol::object& name) {
    if (name.get_type() != sol::type::string) {
        fail(state, std::format("{}: name must be a string", function));
    }
    return name.as<std::string>();
}

std::size_t text_length(const core::FieldValue& value) noexcept {
    if (const auto* text = std::get_if<std::string>(&value)) {
        return text->size();
    }
    if (const auto* asset = std::get_if<core::AssetName>(&value)) {
        return asset->path.size();
    }
    return 0;
}

} // namespace

sol::object Events::declare(sol::this_state state, const sol::object& name) {
    std::string event = checked_name(state, "event", name);
    if (!is_valid_event_name(event)) {
        fail(state, std::format("event: '{}' is not a valid event name: 1 to {} letters, digits, "
                                "'_', '-' or '.', starting with a letter",
                                event, max_event_name_length));
    }
    return sol::make_object(
        state, [this, event = std::move(event)](sol::this_state inner, const sol::object& schema) {
            define(inner, event, schema);
        });
}

void Events::define(sol::this_state state, const std::string& name, const sol::object& schema) {
    if (!_declarations_allowed) {
        fail(state, std::format("event '{}' can only be declared while the scripts load", name));
    }
    if (_schemas.contains(name)) {
        fail(state, std::format("event '{}' is already declared", name));
    }
    if (schema.get_type() != sol::type::table) {
        fail(state, std::format("event '{}' expects a table of argument kinds", name));
    }
    Schema arguments;
    for (const auto& [key, kind] : schema.as<sol::table>()) {
        if (key.get_type() != sol::type::string || !is_valid_key(key.as<std::string>())) {
            fail(state, std::format("event '{}': argument keys must be identifiers of 1 to {} "
                                    "letters, digits or '_'",
                                    name, max_event_name_length));
        }
        const auto argument = key.as<std::string>();
        const auto parsed = kind.get_type() == sol::type::string
                                ? event_kind(kind.as<std::string>())
                                : std::nullopt;
        if (!parsed) {
            fail(state, std::format("event '{}': argument '{}' must be one of {}", name, argument,
                                    event_kind_names()));
        }
        arguments.emplace(argument, *parsed);
    }
    if (arguments.size() > max_event_arguments) {
        fail(state,
             std::format("event '{}' has more than {} arguments", name, max_event_arguments));
    }
    _schemas.emplace(name, std::move(arguments));
}

void Events::emit(sol::this_state state, const sol::object& name, const sol::object& arguments) {
    std::string event = checked_name(state, "emit", name);
    const auto schema = _schemas.find(event);
    if (schema == _schemas.end()) {
        fail(state, std::format("emit: unknown event '{}', declare it with event", event));
    }
    const sol::type type = arguments.get_type();
    if (type != sol::type::table && type != sol::type::lua_nil && type != sol::type::none) {
        fail(state, std::format("emit: arguments of event '{}' must be a table or nil", event));
    }
    std::map<std::string, core::FieldValue, std::less<>> values;
    if (type == sol::type::table) {
        for (const auto& [key, value] : arguments.as<sol::table>()) {
            if (key.get_type() != sol::type::string) {
                fail(state,
                     std::format("emit: argument keys of event '{}' must be strings", event));
            }
            const auto declared = schema->second.find(key.as<std::string>());
            if (declared == schema->second.end()) {
                fail(state, std::format("emit: event '{}' has no argument '{}'", event,
                                        key.as<std::string>()));
            }
            auto converted = from_lua(state, value, declared->second,
                                      std::format("event '{}' argument", event), declared->first);
            if (text_length(converted) > max_event_text_length) {
                fail(state, std::format("emit: event '{}' argument '{}' is longer than {} bytes",
                                        event, declared->first, max_event_text_length));
            }
            values.emplace(declared->first, std::move(converted));
        }
    }
    EventCommand command{.name = std::move(event), .arguments = {}};
    command.arguments.reserve(values.size());
    for (auto& [key, value] : values) {
        command.arguments.push_back({.key = key, .value = std::move(value)});
    }
    queue(std::move(command));
}

std::vector<std::string> Events::names() const {
    std::vector<std::string> names;
    names.reserve(_schemas.size());
    for (const auto& [name, schema] : _schemas) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> Events::keys() const {
    std::set<std::string, std::less<>> keys;
    for (const auto& [name, schema] : _schemas) {
        for (const auto& [key, kind] : schema) {
            keys.insert(key);
        }
    }
    return {keys.begin(), keys.end()};
}

std::vector<EventCommand> Events::take_commands() noexcept {
    _overflow_reported = false;
    return std::exchange(_commands, {});
}

void Events::queue(EventCommand command) {
    if (_commands.size() < max_queued_events) {
        _commands.push_back(std::move(command));
        return;
    }
    if (!_overflow_reported) {
        _overflow_reported = true;
        log::warn("lua: more than {} events emitted since the last tick, dropping the rest",
                  max_queued_events);
    }
}

} // namespace engine::script
