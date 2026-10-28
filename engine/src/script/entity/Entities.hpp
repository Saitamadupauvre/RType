#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "entity/EntityFields.hpp"
#include "loading/Declarations.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

struct EntityHandle {
    core::Entity entity;
};

struct HandleField {
    std::function<sol::object(sol::this_state, core::Entity)> read;
    std::function<void(sol::this_state, core::Entity, const sol::object&)> write;
};

class Entities {
public:
    Entities(sol::state& lua, const core::FieldRegistry& fields);

    void add_handle_field(std::string name, HandleField field);
    void on_release(std::function<void(core::Entity)> listener);

    void attach(core::EntityManager& world);
    [[nodiscard]] core::EntityManager* world() const noexcept { return _world; }
    core::Entity prototype(core::EntityManager& scratch, const Declaration& declaration);
    std::optional<core::Entity> spawn(const Declaration& declaration,
                                      std::optional<core::Vec2> position);
    sol::object spawn_from_lua(sol::this_state state, const Declaration& declaration,
                               const sol::optional<sol::table>& overrides);
    void finish_spawn(core::Entity entity);
    void release(core::Entity entity);
    bool call(core::Entity entity, std::string_view method);
    void update(float dt);
    void collide(core::Entity first, core::Entity second);
    std::size_t flush_destroyed();
    void doom(core::Entity entity);
    [[nodiscard]] std::optional<std::string> type_of(core::Entity entity) const;
    [[nodiscard]] std::optional<std::string> name_of(core::Entity entity) const;
    bool rename(core::Entity entity, std::string name);
    [[nodiscard]] sol::object handle_of(core::Entity entity) const;
    [[nodiscard]] std::vector<core::Entity> live_entities() const;
    [[nodiscard]] bool is_active(core::Entity entity) const;

private:
    struct Record {
        std::string type;
        sol::table data;
        sol::table prefab;
        sol::object handle;
        double age = 0.0;
        bool doomed = false;
        std::string name{};
    };

    template <typename... Args>
    bool invoke(core::Entity entity, std::string_view method, Args&&... args);
    sol::object get(sol::this_state state, const EntityHandle& handle, const sol::object& key);
    void set(sol::this_state state, const EntityHandle& handle, const sol::object& key,
             const sol::object& value);
    void apply_prefab_fields(core::EntityManager& world, core::Entity entity,
                             const sol::table& prefab);
    sol::table own_copies(const sol::table& prefab);
    Record& live_record(sol::this_state state, const EntityHandle& handle);
    void destroy(sol::this_state state, const EntityHandle& handle);
    bool has_tag(sol::this_state state, const EntityHandle& handle, std::string_view label);
    sol::object get_field(sol::this_state state, const EntityHandle& handle,
                          std::string_view component, std::string_view field);
    void set_field(sol::this_state state, const EntityHandle& handle, std::string_view component,
                   std::string_view field, const sol::object& value);
    [[nodiscard]] bool wants_collision(core::Entity entity, core::Entity other) const;

    sol::state& _lua;
    EntityFields _fields;
    std::map<std::string, HandleField, std::less<>> _handle_fields;
    std::vector<std::function<void(core::Entity)>> _release_listeners;
    core::EntityManager* _world = nullptr;
    std::unordered_map<core::Entity::Id, Record> _records;
    std::vector<core::Entity> _doomed;
    sol::table _methods;
};

} // namespace engine::script
