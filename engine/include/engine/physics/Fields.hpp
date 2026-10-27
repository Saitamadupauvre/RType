#pragma once

#include "engine/core/FieldRegistry.hpp"
#include "engine/physics/Export.hpp"

namespace engine::physics {

/**
 * @brief Registers the fields of PhysicsBody, AABBCollider and CircleCollider.
 *
 * @note PhysicsBody's `type` is an int: 0 for static, 1 for dynamic. Other values are rejected
 * with std::out_of_range.
 * @throws core::DuplicateComponentName If one of them is already registered.
 */
ENGINE_PHYSICS_EXPORT void register_physics_fields(core::FieldRegistry& registry);

} // namespace engine::physics
