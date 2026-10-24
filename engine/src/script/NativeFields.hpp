#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "Sol.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"

namespace engine::script {

struct CollisionFilter {
    core::Tags tags;
};

using FieldReader = sol::object (*)(sol::this_state state, core::EntityManager& world,
                                    core::Entity entity);
using FieldWriter = void (*)(sol::this_state state, core::EntityManager& world, core::Entity entity,
                             const sol::object& value);

struct NativeField {
    std::string_view name;
    FieldReader read;
    FieldWriter write;
};

std::span<const NativeField> native_fields();

std::optional<NativeField> find_native_field(std::string_view name);

} // namespace engine::script
