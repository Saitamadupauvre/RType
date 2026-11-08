#include "Declarations.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::array<std::string_view, 31> reserved_globals{
    "entity", "system", "query",    "spawn",   "find_all", "find_first",  "count",      "after",
    "every",  "wait",   "play",     "music",   "sounds",   "musics",      "event",      "emit",
    "random", "world",  "log",      "synced",  "players",  "find_player", "on",         "input",
    "audio",  "camera", "textures", "shaders", "navgrid",  "find_path",   "load_scene",
};

constexpr std::array<std::string_view, 14> engine_owned_fields{"age",
                                                               "id",
                                                               "type",
                                                               "input",
                                                               "owner",
                                                               "destroy",
                                                               "has_tag",
                                                               "get_field",
                                                               "set_field",
                                                               "server",
                                                               "client",
                                                               "navigate_to",
                                                               "stop_navigation",
                                                               "is_navigating"};

constexpr std::array<std::string_view, 6> callbacks{"on_spawn",   "update",   "on_collide",
                                                    "on_destroy", "behavior", "on_arrive"};

constexpr std::size_t callback_prefix_distance = 4;
constexpr std::size_t callback_distance = 2;

template <std::size_t Size>
bool contains(const std::array<std::string_view, Size>& names, std::string_view name) {
    return std::ranges::find(names, name) != names.end();
}

std::size_t edit_distance(std::string_view from, std::string_view to) {
    std::vector<std::size_t> row(to.size() + 1);
    for (std::size_t column = 0; column < row.size(); ++column) {
        row[column] = column;
    }
    for (std::size_t line = 1; line <= from.size(); ++line) {
        std::size_t diagonal = row[0];
        row[0] = line;
        for (std::size_t column = 1; column <= to.size(); ++column) {
            const std::size_t above = row[column];
            const std::size_t change = from[line - 1] == to[column - 1] ? 0 : 1;
            row[column] = std::min({above + 1, row[column - 1] + 1, diagonal + change});
            diagonal = above;
        }
    }
    return row[to.size()];
}

std::optional<std::string_view> misspelled_callback(std::string_view name) {
    const std::size_t limit =
        name.starts_with("on_") ? callback_prefix_distance : callback_distance;
    std::optional<std::string_view> closest;
    std::size_t best = limit + 1;
    for (const std::string_view callback : callbacks) {
        if (const std::size_t distance = edit_distance(name, callback); distance < best) {
            best = distance;
            closest = callback;
        }
    }
    return closest;
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
    } else if (value.get_type() == sol::type::function && !contains(callbacks, field)) {
        if (const auto callback = misspelled_callback(field)) {
            log::warn("lua: {}: {} '{}' function '{}' is never called by the engine; did you mean "
                      "'{}'?",
                      source, kind_name(declaration.kind), declaration.name, field, *callback);
        }
    }
}

void warn_outside_tables(const PendingFile& file, const sol::table& environment) {
    const std::string source = file.source.generic_string();
    std::vector<const PendingDeclaration*> declarations;
    declarations.reserve(file.declarations.size());
    for (const PendingDeclaration& declaration : file.declarations) {
        declarations.push_back(&declaration);
    }
    const std::string owners =
        std::format("{} {}", declarations.size() == 1 ? "the table of" : "the tables of",
                    quoted_names(declarations));
    environment.for_each([&](const sol::object& key, const sol::object&) {
        if (key.get_type() != sol::type::string) {
            return;
        }
        log::warn("lua: {}: '{}' is defined outside {} and is ignored; move it into the table, or "
                  "use the short form for a single declaration",
                  source, key.as<std::string>(), owners);
    });
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
    if (short_forms.empty() && !file.declarations.empty()) {
        warn_outside_tables(file, environment);
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

bool read_persistence(Declaration& declaration) {
    const auto value = declaration.prefab.raw_get<sol::object>(persistent_field);
    if (declaration.kind != DeclarationKind::Entity || value.get_type() == sol::type::lua_nil ||
        value.get_type() == sol::type::none) {
        return true;
    }
    if (value.get_type() != sol::type::boolean) {
        log::error("lua: {}: entity '{}' field '{}' must be a boolean",
                   declaration.source.generic_string(), declaration.name, persistent_field);
        return false;
    }
    declaration.persistent = value.as<bool>();
    return true;
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
