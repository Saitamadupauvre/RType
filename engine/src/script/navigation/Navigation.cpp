#include "navigation/Navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/physics/Components.hpp"
#include "entity/ScriptBody.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

constexpr std::array<std::string_view, 11> grid_options{
    "x",       "y",         "width",     "height",    "cell",  "obstacles",
    "padding", "heuristic", "diagonals", "max_nodes", "smooth"};
constexpr std::array<std::string_view, 2> path_options{"avoid", "max_nodes"};

constexpr float default_cell = 32.0F;
constexpr double max_search_nodes = 10'000'000.0;
constexpr float arrival_distance = 0.01F;

[[noreturn]] void fail(sol::this_state state, std::string_view caller, std::string_view message) {
    throw std::runtime_error(std::format("{}{}: {}", caller_location(state), caller, message));
}

bool is_nil(const sol::object& value) {
    return value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none;
}

template <std::size_t Size>
sol::table options_table(sol::this_state state, const sol::object& value, std::string_view caller,
                         const std::array<std::string_view, Size>& allowed) {
    if (is_nil(value)) {
        return sol::state_view(state).create_table();
    }
    if (value.get_type() != sol::type::table) {
        fail(state, caller, "options must be a table");
    }
    const auto table = value.as<sol::table>();
    for (const auto& [key, unused] : table) {
        if (key.get_type() != sol::type::string ||
            std::ranges::find(allowed, key.as<std::string_view>()) == allowed.end()) {
            fail(state, caller,
                 key.get_type() == sol::type::string
                     ? std::format("unknown option '{}'", key.as<std::string_view>())
                     : std::string("option names must be strings"));
        }
    }
    return table;
}

std::optional<float> read_number(sol::this_state state, const sol::table& table,
                                 std::string_view key, std::string_view caller) {
    const auto value = table.get<sol::object>(key);
    if (is_nil(value)) {
        return std::nullopt;
    }
    if (value.get_type() != sol::type::number || !std::isfinite(value.as<double>())) {
        fail(state, caller, std::format("'{}' must be a finite number", key));
    }
    return value.as<float>();
}

float positive_number(sol::this_state state, const sol::table& table, std::string_view key,
                      std::optional<float> fallback, std::string_view caller) {
    const auto value = read_number(state, table, key, caller);
    if (!value && !fallback) {
        fail(state, caller, std::format("'{}' is required", key));
    }
    const float number = value.value_or(fallback.value_or(0.0F));
    if (number <= 0.0F) {
        fail(state, caller, std::format("'{}' must be positive", key));
    }
    return number;
}

bool read_bool(sol::this_state state, const sol::table& table, std::string_view key, bool fallback,
               std::string_view caller) {
    const auto value = table.get<sol::object>(key);
    if (is_nil(value)) {
        return fallback;
    }
    if (value.get_type() != sol::type::boolean) {
        fail(state, caller, std::format("'{}' must be a boolean", key));
    }
    return value.as<bool>();
}

std::size_t read_count(sol::this_state state, const sol::table& table, std::string_view key,
                       std::size_t fallback, std::string_view caller) {
    const auto value = read_number(state, table, key, caller);
    if (!value) {
        return fallback;
    }
    const double number = table.get<double>(key);
    if (number < 1.0 || number > max_search_nodes || std::floor(number) != number) {
        fail(state, caller,
             std::format("'{}' must be a whole number from 1 to {}", key,
                         static_cast<std::size_t>(max_search_nodes)));
    }
    return static_cast<std::size_t>(number);
}

std::vector<std::string> read_tags(sol::this_state state, const sol::table& table,
                                   std::string_view key, std::string_view caller) {
    const auto value = table.get<sol::object>(key);
    if (is_nil(value)) {
        return {};
    }
    const auto invalid = std::format("'{}' must be an array of tags", key);
    if (value.get_type() != sol::type::table) {
        fail(state, caller, invalid);
    }
    const auto list = value.as<sol::table>();
    std::vector<std::string> tags;
    for (std::size_t index = 1; index <= list.size(); ++index) {
        const auto tag = list.get<sol::object>(index);
        if (tag.get_type() != sol::type::string) {
            fail(state, caller, invalid);
        }
        tags.push_back(tag.as<std::string>());
    }
    return tags;
}

navigation::Heuristic read_heuristic(sol::this_state state, const sol::table& table,
                                     std::string_view caller) {
    const auto value = table.get<sol::object>("heuristic");
    if (is_nil(value)) {
        return navigation::Heuristic::Octile;
    }
    const auto heuristic = value.get_type() == sol::type::string
                               ? navigation::parse_heuristic(value.as<std::string_view>())
                               : std::nullopt;
    if (!heuristic) {
        fail(state, caller,
             R"('heuristic' must be "octile", "manhattan", "euclidean" or "dijkstra")");
    }
    return *heuristic;
}

int cells_along(float length, float cell) {
    return static_cast<int>(std::ceil(static_cast<double>(length) / cell));
}

bool has_any_tag(const core::Tags& tags, const std::vector<std::string>& wanted) {
    return std::ranges::any_of(wanted, [&](const std::string& tag) { return tags.has(tag); });
}

float distance(navigation::Point from, navigation::Point to) {
    return std::hypot(to.x - from.x, to.y - from.y);
}

} // namespace

Navigation::Navigation(Entities& entities) : _entities(entities) {
    _entities.add_method("navigate_to", [this](sol::this_state state, const EntityHandle& handle,
                                               const sol::object& to, const sol::object& speed) {
        return navigate_to(state, handle, to, speed);
    });
    _entities.add_method(
        "stop_navigation",
        [this](sol::this_state state, const EntityHandle& handle) { stop(state, handle); });
    _entities.add_method("is_navigating",
                         [this](sol::this_state state, const EntityHandle& handle) {
                             return is_navigating(state, handle);
                         });
    _entities.on_release([this](core::Entity entity) { _agents.erase(entity.id()); });
}

void Navigation::configure(sol::this_state state, const sol::object& options) {
    constexpr std::string_view caller = "navgrid";
    if (is_nil(options)) {
        fail(state, caller, "options must be a table");
    }
    const sol::table table = options_table(state, options, caller, grid_options);
    Settings settings{.origin = {.x = read_number(state, table, "x", caller).value_or(0.0F),
                                 .y = read_number(state, table, "y", caller).value_or(0.0F)},
                      .size = {.x = positive_number(state, table, "width", std::nullopt, caller),
                               .y = positive_number(state, table, "height", std::nullopt, caller)},
                      .cell = positive_number(state, table, "cell", default_cell, caller),
                      .obstacles = read_tags(state, table, "obstacles", caller),
                      .padding = read_number(state, table, "padding", caller).value_or(0.0F),
                      .path = {.heuristic = read_heuristic(state, table, caller),
                               .diagonals = read_bool(state, table, "diagonals", true, caller),
                               .max_expanded = read_count(state, table, "max_nodes",
                                                          navigation::default_max_expanded, caller),
                               .smooth = read_bool(state, table, "smooth", true, caller)}};
    if (settings.padding < 0.0F) {
        fail(state, caller, "'padding' must not be negative");
    }
    const double columns = std::ceil(static_cast<double>(settings.size.x) / settings.cell);
    const double rows = std::ceil(static_cast<double>(settings.size.y) / settings.cell);
    if (columns * rows > static_cast<double>(navigation::NavGrid::max_cells)) {
        fail(state, caller,
             std::format("a grid of {} x {} cells exceeds {} cells", columns, rows,
                         navigation::NavGrid::max_cells));
    }
    navigation::NavGrid grid(cells_along(settings.size.x, settings.cell),
                             cells_along(settings.size.y, settings.cell), settings.cell,
                             settings.origin);
    _configured.emplace(Configured{.settings = std::move(settings), .grid = std::move(grid)});
    _stale = true;
}

sol::object Navigation::find_path(sol::this_state state, const sol::object& from,
                                  const sol::object& to, const sol::object& options) {
    constexpr std::string_view caller = "find_path";
    const navigation::Point start = point_of(state, from, caller, "from");
    const navigation::Point goal = point_of(state, to, caller, "to");
    const Configured& current = configured(state, caller);
    const auto points =
        plan(current, start, goal, query_of(state, options, current.settings.path.max_expanded));
    if (!points) {
        return sol::make_object(state, sol::lua_nil);
    }
    sol::state_view lua(state);
    sol::table result = lua.create_table(static_cast<int>(points->size()), 0);
    for (std::size_t index = 0; index < points->size(); ++index) {
        const navigation::Point point = (*points)[index];
        result[index + 1] = lua.create_table_with("x", point.x, "y", point.y);
    }
    return result;
}

void Navigation::new_tick() noexcept { _stale = true; }

std::vector<core::Entity> Navigation::step(float dt) {
    std::vector<core::Entity> arrived;
    if (!std::isfinite(dt) || dt <= 0.0F) {
        return arrived;
    }
    for (auto agent = _agents.begin(); agent != _agents.end();) {
        if (!_entities.is_active(agent->second.entity)) {
            agent = _agents.erase(agent);
        } else if (steer(agent->second, dt)) {
            arrived.push_back(agent->second.entity);
            agent = _agents.erase(agent);
        } else {
            ++agent;
        }
    }
    return arrived;
}

bool Navigation::navigate_to(sol::this_state state, const EntityHandle& handle,
                             const sol::object& to, const sol::object& speed) {
    constexpr std::string_view caller = "navigate_to";
    require_active(state, handle.entity, caller);
    const navigation::Point goal = point_of(state, to, caller, "to");
    if (speed.get_type() != sol::type::number || !std::isfinite(speed.as<double>()) ||
        speed.as<double>() <= 0.0) {
        fail(state, caller, "speed must be a positive number");
    }
    const Configured& current = configured(state, caller);
    auto points = plan(current, center_of(handle.entity), goal,
                       Query{.avoid = {}, .max_nodes = current.settings.path.max_expanded});
    if (!points) {
        stop(state, handle);
        return false;
    }
    _agents.insert_or_assign(handle.entity.id(), Agent{.entity = handle.entity,
                                                       .waypoints = std::move(*points),
                                                       .next = 0,
                                                       .speed = speed.as<float>()});
    return true;
}

void Navigation::stop(sol::this_state state, const EntityHandle& handle) {
    require_active(state, handle.entity, "stop_navigation");
    _agents.erase(handle.entity.id());
    core::EntityManager& world = *_entities.world();
    if (world.has<physics::PhysicsBody>(handle.entity)) {
        world.get<physics::PhysicsBody>(handle.entity).velocity = {0.0F, 0.0F};
    }
}

bool Navigation::is_navigating(sol::this_state state, const EntityHandle& handle) const {
    require_active(state, handle.entity, "is_navigating");
    return _agents.contains(handle.entity.id());
}

void Navigation::require_active(sol::this_state state, core::Entity entity,
                                std::string_view caller) const {
    if (!_entities.is_active(entity)) {
        fail(state, caller, "the entity is destroyed");
    }
}

Navigation::Query Navigation::query_of(sol::this_state state, const sol::object& options,
                                       std::size_t max_nodes) {
    constexpr std::string_view caller = "find_path";
    const sol::table table = options_table(state, options, caller, path_options);
    return Query{.avoid = read_tags(state, table, "avoid", caller),
                 .max_nodes = read_count(state, table, "max_nodes", max_nodes, caller)};
}

navigation::Point Navigation::point_of(sol::this_state state, const sol::object& value,
                                       std::string_view caller, std::string_view argument) const {
    if (value.is<EntityHandle>()) {
        const core::Entity entity = value.as<EntityHandle>().entity;
        require_active(state, entity, caller);
        return center_of(entity);
    }
    const auto invalid =
        std::format("'{}' must be an entity or a table {{ x = .., y = .. }}", argument);
    if (value.get_type() != sol::type::table) {
        fail(state, caller, invalid);
    }
    const auto table = value.as<sol::table>();
    const auto x = table.get<sol::object>("x");
    const auto y = table.get<sol::object>("y");
    if (x.get_type() != sol::type::number || y.get_type() != sol::type::number ||
        !std::isfinite(x.as<double>()) || !std::isfinite(y.as<double>())) {
        fail(state, caller, invalid);
    }
    return {.x = x.as<float>(), .y = y.as<float>()};
}

navigation::Point Navigation::center_of(core::Entity entity) const {
    core::EntityManager& world = *_entities.world();
    core::Vec2 center = world.get<core::Position>(entity).pos;
    if (world.has<physics::AABBCollider>(entity)) {
        center += world.get<physics::AABBCollider>(entity).offset;
    } else if (world.has<physics::CircleCollider>(entity)) {
        center += world.get<physics::CircleCollider>(entity).offset;
    }
    return {.x = center.x, .y = center.y};
}

Navigation::Configured& Navigation::configured(sol::this_state state, std::string_view caller) {
    if (!_configured.has_value()) {
        fail(state, caller, "call navgrid { ... } first");
    }
    Configured& current = _configured.value();
    if (_stale) {
        rebuild(current);
        _stale = false;
    }
    return current;
}

void Navigation::rebuild(Configured& current) const {
    current.grid.reset();
    block_tagged(current.grid, current.settings.obstacles, current.settings.padding);
}

void Navigation::block_tagged(navigation::NavGrid& grid, const std::vector<std::string>& tags,
                              float padding) const {
    core::EntityManager* world = _entities.world();
    if (tags.empty() || world == nullptr) {
        return;
    }
    world->query<core::Position, core::Tags>().each(
        [&](core::Entity entity, const core::Position& position, const core::Tags& labels) {
            if (!has_any_tag(labels, tags)) {
                return;
            }
            core::Vec2 half;
            core::Vec2 center = position.pos;
            if (world->has<physics::AABBCollider>(entity)) {
                const auto& collider = world->get<physics::AABBCollider>(entity);
                center += collider.offset;
                half = collider.size / 2.0F;
            } else if (world->has<physics::CircleCollider>(entity)) {
                const auto& collider = world->get<physics::CircleCollider>(entity);
                center += collider.offset;
                half = {collider.radius, collider.radius};
            } else {
                return;
            }
            half += core::Vec2{padding, padding};
            grid.block_area({.x = center.x - half.x, .y = center.y - half.y},
                            {.x = center.x + half.x, .y = center.y + half.y});
        });
}

std::optional<std::vector<navigation::Point>> Navigation::plan(const Configured& current,
                                                               navigation::Point from,
                                                               navigation::Point to,
                                                               const Query& query) const {
    std::optional<navigation::NavGrid> avoiding;
    if (!query.avoid.empty()) {
        block_tagged(avoiding.emplace(current.grid), query.avoid, current.settings.padding);
    }
    const navigation::NavGrid& grid = avoiding.has_value() ? avoiding.value() : current.grid;
    const auto start = grid.cell_at(from);
    const auto goal = grid.cell_at(to);
    if (!start || !goal) {
        return std::nullopt;
    }
    navigation::PathOptions options = current.settings.path;
    options.max_expanded = query.max_nodes;
    const auto result = navigation::find_path(grid, *start, *goal, options);
    if (result.status != navigation::PathStatus::Found) {
        return std::nullopt;
    }
    std::vector<navigation::Point> points;
    for (std::size_t index = 1; index + 1 < result.cells.size(); ++index) {
        points.push_back(grid.center_of(result.cells[index]));
    }
    points.push_back(to);
    return points;
}

bool Navigation::steer(Agent& agent, float dt) {
    const navigation::Point center = center_of(agent.entity);
    while (agent.next < agent.waypoints.size() &&
           distance(center, agent.waypoints[agent.next]) <= arrival_distance) {
        ++agent.next;
    }
    physics::PhysicsBody& body = script_body(*_entities.world(), agent.entity);
    if (agent.next == agent.waypoints.size()) {
        body.velocity = {0.0F, 0.0F};
        return true;
    }
    const navigation::Point target = agent.waypoints[agent.next];
    const core::Vec2 delta{target.x - center.x, target.y - center.y};
    const float remaining = delta.length();
    body.velocity = remaining <= agent.speed * dt ? delta / dt : delta * (agent.speed / remaining);
    return false;
}

} // namespace engine::script
