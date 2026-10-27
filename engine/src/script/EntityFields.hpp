#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "Sol.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"

namespace engine::script {

enum class AliasShape : std::uint8_t { Whole, AxisX, AxisY, Size };

struct FieldAlias {
    std::string_view name;
    std::string_view component;
    std::string_view field;
    AliasShape shape = AliasShape::Whole;
    bool (*lacks_defaults)(const core::EntityManager&, core::Entity) = nullptr;
    void (*apply_defaults)(core::EntityManager&, core::Entity) = nullptr;
    void (*after_set)(core::EntityManager&, core::Entity) = nullptr;
};

class EntityFields {
public:
    explicit EntityFields(const core::FieldRegistry& registry);

    [[nodiscard]] bool has_alias(std::string_view name) const;
    [[nodiscard]] std::vector<std::string_view> alias_names() const;

    sol::object read_alias(sol::this_state state, core::EntityManager& world, core::Entity entity,
                           std::string_view name) const;
    void write_alias(sol::this_state state, core::EntityManager& world, core::Entity entity,
                     std::string_view name, const sol::object& value) const;

    sol::object read(sol::this_state state, core::EntityManager& world, core::Entity entity,
                     std::string_view component, std::string_view field) const;
    void write(sol::this_state state, core::EntityManager& world, core::Entity entity,
               std::string_view component, std::string_view field, const sol::object& value) const;

private:
    struct Binding {
        FieldAlias alias;
        core::FieldRef field;
    };

    [[nodiscard]] const Binding* find(std::string_view name) const;
    [[nodiscard]] core::FieldRef require(sol::this_state state, std::string_view component,
                                         std::string_view field) const;
    sol::object read_field(sol::this_state state, core::EntityManager& world, core::Entity entity,
                           core::FieldRef field) const;
    void write_value(sol::this_state state, core::EntityManager& world, core::Entity entity,
                     core::FieldRef field, std::string_view name,
                     const core::FieldValue& value) const;
    [[nodiscard]] core::FieldValue alias_value(sol::this_state state, core::EntityManager& world,
                                               core::Entity entity, const Binding& binding,
                                               const sol::object& value) const;

    const core::FieldRegistry& _registry;
    std::vector<Binding> _bindings;
};

} // namespace engine::script
