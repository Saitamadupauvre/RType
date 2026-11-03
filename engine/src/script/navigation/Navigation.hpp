#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/navigation/NavGrid.hpp"
#include "engine/navigation/Pathfinding.hpp"
#include "entity/Entities.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class Navigation {
public:
    explicit Navigation(Entities& entities);

    void configure(sol::this_state state, const sol::object& options);
    sol::object find_path(sol::this_state state, const sol::object& from, const sol::object& to,
                          const sol::object& options);
    void new_tick() noexcept;
    std::vector<core::Entity> step(float dt);

private:
    struct Settings {
        navigation::Point origin;
        navigation::Point size;
        float cell{0.0F};
        std::vector<std::string> obstacles;
        float padding{0.0F};
        navigation::PathOptions path;
    };

    struct Configured {
        Settings settings;
        navigation::NavGrid grid;
    };

    struct Query {
        std::vector<std::string> avoid;
        std::size_t max_nodes{0};
    };

    struct Agent {
        core::Entity entity;
        std::vector<navigation::Point> waypoints;
        std::size_t next{0};
        float speed{0.0F};
    };

    bool navigate_to(sol::this_state state, const EntityHandle& handle, const sol::object& to,
                     const sol::object& speed);
    void stop(sol::this_state state, const EntityHandle& handle);
    [[nodiscard]] bool is_navigating(sol::this_state state, const EntityHandle& handle) const;
    void require_active(sol::this_state state, core::Entity entity, std::string_view caller) const;
    [[nodiscard]] static Query query_of(sol::this_state state, const sol::object& options,
                                        std::size_t max_nodes);
    [[nodiscard]] navigation::Point point_of(sol::this_state state, const sol::object& value,
                                             std::string_view caller,
                                             std::string_view argument) const;
    [[nodiscard]] navigation::Point center_of(core::Entity entity) const;
    Configured& configured(sol::this_state state, std::string_view caller);
    void rebuild(Configured& current) const;
    void block_tagged(navigation::NavGrid& grid, const std::vector<std::string>& tags,
                      float padding) const;
    [[nodiscard]] std::optional<std::vector<navigation::Point>> plan(const Configured& current,
                                                                     navigation::Point from,
                                                                     navigation::Point to,
                                                                     const Query& query) const;
    bool steer(Agent& agent, float dt);

    Entities& _entities;
    std::optional<Configured> _configured;
    bool _stale{true};
    std::map<core::Entity::Id, Agent> _agents;
};

} // namespace engine::script
