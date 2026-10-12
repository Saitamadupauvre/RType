#include "Declarations.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::array<std::string_view, 16> reserved_globals{
    "entity", "system", "query", "spawn", "find_all", "find_first", "count", "after",
    "every",  "wait",   "play",  "music", "random",   "world",      "log",   "synced",
};

constexpr std::array<std::string_view, 3> engine_owned_fields{"age", "id", "type"};

constexpr std::array<std::string_view, 5> callbacks{"on_spawn", "update", "on_hit", "on_destroy",
                                                    "behavior"};

template <std::size_t Size>
bool contains(const std::array<std::string_view, Size>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

std::string quoted_names(const std::vector<const PendingDeclaration*>& declarations) {
    std::string result;
    for (const PendingDeclaration* declaration : declarations) {
        result += result.empty() ? "" : ", ";
        result += std::format("'{}'", declaration->name);
    }
    return result;
}

void warn_reserved_field(const Declaration& declaration, std::string_view field,
                         const sol::object& value) {
    const std::string source = declaration.source.generic_string();
    if (contains(reserved_globals, field) || contains(engine_owned_fields, field)) {
        log::warn("lua: {}: {} '{}' defines reserved name '{}'", source,
                  kind_name(declaration.kind), declaration.name, field);
    } else if (contains(callbacks, field) && value.get_type() != sol::type::function) {
        log::warn("lua: {}: {} '{}' callback '{}' must be a function", source,
                  kind_name(declaration.kind), declaration.name, field);
    }
}

} // namespace

DeclarationScope::DeclarationScope(std::shared_ptr<PendingFile>& slot,
                                   std::shared_ptr<PendingFile> file)
    : _slot(&slot), _previous(std::exchange(slot, std::move(file))) {}

DeclarationScope::~DeclarationScope() {
    if (*_slot) {
        (*_slot)->open = false;
    }
    *_slot = std::move(_previous);
}

std::string_view kind_name(DeclarationKind kind) {
    return kind == DeclarationKind::Entity ? "entity" : "system";
}

std::string caller_location(lua_State* state) {
    luaL_where(state, 1);
    std::string location = lua_tostring(state, -1);
    lua_pop(state, 1);
    return location;
}

sol::object make_declarator(sol::this_state state, const std::shared_ptr<PendingFile>& file,
                            std::size_t index) {
    const std::string name = file->declarations[index].name;
    return sol::make_object(
        state, [file, index, name](sol::this_state call, const sol::table& body) {
            if (!file->open) {
                throw std::runtime_error(
                    std::format("{}'{}' can only receive its body while {} is loading",
                                caller_location(call), name, file->source.generic_string()));
            }
            PendingDeclaration& declaration = file->declarations[index];
            if (declaration.body) {
                throw std::runtime_error(
                    std::format("{}'{}' already has a body", caller_location(call), name));
            }
            declaration.body = body;
        });
}

std::optional<std::vector<Declaration>> resolve_declarations(const PendingFile& file,
                                                             const sol::table& environment) {
    const std::string source = file.source.generic_string();
    std::vector<const PendingDeclaration*> short_forms;
    for (const PendingDeclaration& declaration : file.declarations) {
        if (!declaration.body) {
            short_forms.push_back(&declaration);
        }
    }
    if (short_forms.size() > 1) {
        log::error("lua: {}: only one short-form declaration is allowed per file, found {}", source,
                   quoted_names(short_forms));
        return std::nullopt;
    }
    if (short_forms.size() == 1 && file.declarations.size() > 1) {
        log::error("lua: {}: short-form declaration '{}' cannot be mixed with table-form "
                   "declarations",
                   source, short_forms.front()->name);
        return std::nullopt;
    }
    std::vector<Declaration> declarations;
    declarations.reserve(file.declarations.size());
    for (const PendingDeclaration& pending : file.declarations) {
        declarations.push_back(Declaration{.name = pending.name,
                                           .kind = pending.kind,
                                           .source = file.source,
                                           .prefab = pending.body.value_or(environment)});
    }
    return declarations;
}

void warn_reserved_names(const Declaration& declaration) {
    if (contains(reserved_globals, declaration.name)) {
        log::warn("lua: {}: {} name '{}' is reserved", declaration.source.generic_string(),
                  kind_name(declaration.kind), declaration.name);
    }
    declaration.prefab.for_each([&](const sol::object& key, const sol::object& value) {
        if (key.get_type() == sol::type::string) {
            warn_reserved_field(declaration, key.as<std::string>(), value);
        }
    });
}

} // namespace engine::script
