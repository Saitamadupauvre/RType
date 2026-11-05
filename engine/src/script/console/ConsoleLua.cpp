#include "console/ConsoleLua.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

namespace log = core::log;
using core::console::CVarKind;
using core::console::CVarRange;
using core::console::CVars;
using core::console::CVarSpec;
using core::console::CVarValue;
using core::console::ExecutionResult;

constexpr std::string_view chunk_name = "=console";

bool fail(sol::this_state state, std::string_view function, std::string_view error) {
    log::error("lua: {}cvar.{}: {}", caller_location(state), function, error);
    return false;
}

bool is_integer(sol::this_state state, const sol::object& value) {
    lua_State* lua = state;
    value.push(lua);
    const bool integer = lua_isinteger(lua, -1) != 0;
    lua_pop(lua, 1);
    return integer;
}

std::optional<CVarValue> to_value(sol::this_state state, const sol::object& value) {
    switch (value.get_type()) {
    case sol::type::boolean:
        return value.as<bool>();
    case sol::type::number:
        if (is_integer(state, value)) {
            return value.as<std::int64_t>();
        }
        return value.as<double>();
    case sol::type::string:
        return value.as<std::string>();
    default:
        return std::nullopt;
    }
}

sol::object to_lua(sol::this_state state, const CVarValue& value) {
    return std::visit([&state](const auto& held) { return sol::make_object(state, held); }, value);
}

std::optional<CVarValue> with_kind(const CVarValue& value, std::string_view kind) {
    const auto* integer = std::get_if<std::int64_t>(&value);
    const auto* number = std::get_if<double>(&value);
    if (kind == "number" && (integer != nullptr || number != nullptr)) {
        return integer != nullptr ? static_cast<double>(*integer) : *number;
    }
    if (kind == "int" && integer != nullptr) {
        return value;
    }
    if (kind == "int" && number != nullptr && std::trunc(*number) == *number &&
        std::isfinite(*number)) {
        return static_cast<std::int64_t>(*number);
    }
    if (kind == core::console::kind_name(core::console::kind_of(value))) {
        return value;
    }
    return std::nullopt;
}

std::optional<std::string> name_of(sol::this_state state, std::string_view function,
                                   const sol::object& name) {
    if (name.get_type() != sol::type::string) {
        fail(state, function, "the name must be a string");
        return std::nullopt;
    }
    return name.as<std::string>();
}

std::optional<CVarSpec> read_spec(sol::this_state state, const sol::object& argument) {
    if (argument.get_type() != sol::type::table) {
        fail(state, "register", "expects a table {name = ..., default = ...}");
        return std::nullopt;
    }
    const auto table = argument.as<sol::table>();
    const auto name = name_of(state, "register", table.get<sol::object>("name"));
    if (!name) {
        return std::nullopt;
    }
    auto value = to_value(state, table.get<sol::object>("default"));
    if (!value) {
        fail(state, "register", *name + ": the default must be a boolean, a number or a string");
        return std::nullopt;
    }
    if (const auto kind = table.get<sol::optional<std::string>>("kind")) {
        value = with_kind(*value, *kind);
        if (!value) {
            fail(state, "register", *name + ": the default does not fit the kind " + *kind);
            return std::nullopt;
        }
    }
    CVarSpec spec{.name = *name,
                  .default_value = std::move(*value),
                  .description = table.get_or<std::string>("description", ""),
                  .range = std::nullopt};
    const auto min = table.get<sol::optional<double>>("min");
    const auto max = table.get<sol::optional<double>>("max");
    if (min.has_value() != max.has_value()) {
        fail(state, "register", *name + ": give both min and max, or neither");
        return std::nullopt;
    }
    if (min && max) {
        spec.range = CVarRange{.min = *min, .max = *max};
    }
    return spec;
}

bool register_cvar(CVars& cvars, sol::this_state state, const sol::object& argument) {
    auto spec = read_spec(state, argument);
    if (!spec) {
        return false;
    }
    const auto added = cvars.add(std::move(*spec));
    return added ? true : fail(state, "register", added.error);
}

sol::object get_cvar(const CVars& cvars, sol::this_state state, const sol::object& argument) {
    const auto name = name_of(state, "get", argument);
    const auto value = name ? cvars.get(*name) : std::nullopt;
    if (name && !value) {
        fail(state, "get", "unknown variable " + *name);
    }
    return value ? to_lua(state, *value) : sol::make_object(state, sol::lua_nil);
}

bool set_cvar(CVars& cvars, sol::this_state state, const sol::object& argument,
              const sol::object& lua_value) {
    const auto name = name_of(state, "set", argument);
    if (!name) {
        return false;
    }
    const auto value = to_value(state, lua_value);
    if (!value) {
        return fail(state, "set", *name + ": the value must be a boolean, a number or a string");
    }
    const auto changed = cvars.set(*name, *value);
    return changed ? true : fail(state, "set", changed.error);
}

bool reset_cvar(CVars& cvars, sol::this_state state, const sol::object& argument) {
    const auto name = name_of(state, "reset", argument);
    if (!name) {
        return false;
    }
    const auto reset = cvars.reset(*name);
    return reset ? true : fail(state, "reset", reset.error);
}

sol::table list_cvars(const CVars& cvars, sol::this_state state) {
    sol::state_view lua(state);
    sol::table names = lua.create_table();
    for (const auto& info : cvars.list()) {
        names.add(info.name);
    }
    return names;
}

std::string text_of(const sol::protected_function& tostring, const sol::object& value) {
    const sol::protected_function_result text = tostring(value);
    if (!text.valid() || text.get_type() != sol::type::string) {
        return "?";
    }
    return text.get<std::string>();
}

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string joined;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            joined += separator;
        }
        joined += parts[index];
    }
    return joined;
}

using Output = std::shared_ptr<std::vector<std::string>>;

sol::environment console_scope(sol::state& lua, const sol::table& scope, const Output& output) {
    sol::environment console(lua, sol::create);
    sol::table meta = lua.create_table();
    meta["__index"] = scope;
    meta["__newindex"] = scope;
    console[sol::metatable_key] = meta;
    const sol::protected_function tostring = lua["tostring"];
    console.raw_set("print", [output, tostring](sol::variadic_args arguments) {
        std::vector<std::string> parts;
        for (const auto& argument : arguments) {
            parts.push_back(text_of(tostring, argument.get<sol::object>()));
        }
        output->push_back(join(parts, "\t"));
    });
    return console;
}

std::optional<sol::protected_function> compile(sol::state& lua, const std::string& code,
                                               std::string& error) {
    sol::load_result chunk = lua.load(code, std::string(chunk_name), sol::load_mode::text);
    if (!chunk.valid()) {
        const sol::error failure = chunk;
        error = failure.what();
        return std::nullopt;
    }
    return chunk.get<sol::protected_function>();
}

std::optional<sol::protected_function> compile_line(sol::state& lua, std::string_view code,
                                                    std::string& error) {
    if (auto expression = compile(lua, "return " + std::string(code), error)) {
        return expression;
    }
    return compile(lua, std::string(code), error);
}

} // namespace

void bind_cvars(sol::state& lua, CVars& cvars) {
    sol::table api = lua.create_named_table("cvar");
    api.set_function("register", [&cvars](sol::this_state state, const sol::object& spec) {
        return register_cvar(cvars, state, spec);
    });
    api.set_function("get", [&cvars](sol::this_state state, const sol::object& name) {
        return get_cvar(cvars, state, name);
    });
    api.set_function(
        "set", [&cvars](sol::this_state state, const sol::object& name, const sol::object& value) {
            return set_cvar(cvars, state, name, value);
        });
    api.set_function("reset", [&cvars](sol::this_state state, const sol::object& name) {
        return reset_cvar(cvars, state, name);
    });
    api.set_function("list", [&cvars](sol::this_state state) { return list_cvars(cvars, state); });
}

ExecutionResult evaluate(sol::state& lua, const sol::table& scope, std::string_view code) {
    std::string error;
    auto function = compile_line(lua, code, error);
    if (!function) {
        return {.ok = false, .output = error};
    }
    const auto output = std::make_shared<std::vector<std::string>>();
    const sol::environment console = console_scope(lua, scope, output);
    sol::set_environment(console, *function);
    const sol::protected_function_result result = (*function)();
    if (!result.valid()) {
        const sol::error failure = result;
        output->emplace_back(failure.what());
        return {.ok = false, .output = join(*output, "\n")};
    }
    std::vector<std::string> returned;
    const sol::protected_function tostring = lua["tostring"];
    for (int index = 0; index < result.return_count(); ++index) {
        returned.push_back(text_of(tostring, result.get<sol::object>(index)));
    }
    if (!returned.empty()) {
        output->push_back(join(returned, "\t"));
    }
    return {.ok = true, .output = join(*output, "\n")};
}

} // namespace engine::script
