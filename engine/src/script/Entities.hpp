#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "Declarations.hpp"
#include "Sol.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::script {

struct EntityHandle {
    core::Entity entity;
};

class Entities {
public:
    explicit Entities(sol::state& lua);

    void attach(core::EntityManager& world);
    std::optional<core::Entity> spawn(const Declaration& declaration,
                                      std::optional<core::Vec2> position);
    sol::object spawn_from_lua(sol::this_state state, const Declaration& declaration,
                               const sol::optional<sol::table>& overrides);
    void release(core::Entity entity);
    bool call(core::Entity entity, std::string_view method);

private:
    struct Record {
        std::string type;
        sol::table data;
        sol::table prefab;
        sol::object handle;
    };

    sol::object get(sol::this_state state, const EntityHandle& handle, const sol::object& key);
    void set(sol::this_state state, const EntityHandle& handle, const sol::object& key,
             const sol::object& value);
    void apply_prefab_fields(core::Entity entity, const sol::table& prefab);
    Record& live_record(sol::this_state state, const EntityHandle& handle);

    sol::state& _lua;
    core::EntityManager* _world = nullptr;
    std::unordered_map<core::Entity::Id, Record> _records;
};

} // namespace engine::script
