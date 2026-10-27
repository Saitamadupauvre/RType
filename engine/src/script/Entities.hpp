#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Declarations.hpp"
#include "EntityFields.hpp"
#include "Players.hpp"
#include "Sol.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/InputState.hpp"

namespace engine::script {

struct EntityHandle {
    core::Entity entity;
};

class Entities {
public:
    Entities(sol::state& lua, const Players& players, const core::FieldRegistry& fields);

    void attach(core::EntityManager& world);
    std::optional<core::Entity> spawn(const Declaration& declaration,
                                      std::optional<core::Vec2> position);
    sol::object spawn_from_lua(sol::this_state state, const Declaration& declaration,
                               const sol::optional<sol::table>& overrides);
    void finish_spawn(core::Entity entity);
    void release(core::Entity entity);
    bool call(core::Entity entity, std::string_view method);
    void set_input(core::Entity entity, InputState input);
    void update(float dt);
    void collide(core::Entity first, core::Entity second);
    std::size_t flush_destroyed();
    void doom(core::Entity entity);
    [[nodiscard]] std::vector<core::Entity> owned_by(std::uint32_t player_id) const;
    [[nodiscard]] std::optional<std::string> type_of(core::Entity entity) const;
    [[nodiscard]] std::optional<std::uint32_t> owner_of(core::Entity entity) const;
    [[nodiscard]] sol::object handle_of(core::Entity entity) const;
    [[nodiscard]] sol::object player_object(sol::this_state state, std::uint32_t player_id) const;

private:
    struct Record {
        std::string type;
        sol::table data;
        sol::table prefab;
        sol::object handle;
        InputState input;
        std::optional<std::uint32_t> owner;
        double age = 0.0;
        bool doomed = false;
    };

    template <typename... Args>
    bool invoke(core::Entity entity, std::string_view method, Args&&... args);
    sol::object get(sol::this_state state, const EntityHandle& handle, const sol::object& key);
    void set(sol::this_state state, const EntityHandle& handle, const sol::object& key,
             const sol::object& value);
    void apply_prefab_fields(core::Entity entity, const sol::table& prefab);
    sol::table own_copies(const sol::table& prefab);
    Record& live_record(sol::this_state state, const EntityHandle& handle);
    void destroy(sol::this_state state, const EntityHandle& handle);
    [[nodiscard]] const InputState& input_of(const Record& record) const;
    void set_owner(sol::this_state state, Record& record, const sol::object& value);
    bool has_tag(sol::this_state state, const EntityHandle& handle, std::string_view label);
    sol::object get_field(sol::this_state state, const EntityHandle& handle,
                          std::string_view component, std::string_view field);
    void set_field(sol::this_state state, const EntityHandle& handle, std::string_view component,
                   std::string_view field, const sol::object& value);
    [[nodiscard]] std::vector<core::Entity> live_entities() const;
    [[nodiscard]] bool is_active(core::Entity entity) const;
    [[nodiscard]] bool wants_collision(core::Entity entity, core::Entity other) const;

    sol::state& _lua;
    const Players& _players;
    EntityFields _fields;
    core::EntityManager* _world = nullptr;
    std::unordered_map<core::Entity::Id, Record> _records;
    std::vector<core::Entity> _doomed;
    sol::table _methods;
};

} // namespace engine::script
