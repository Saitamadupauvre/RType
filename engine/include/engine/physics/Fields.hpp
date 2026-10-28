#pragma once

#include "engine/core/FieldRegistry.hpp"
#include "engine/physics/Export.hpp"

namespace engine::physics {

/**
 * @brief Registers the fields of PhysicsBody, AABBCollider, CircleCollider and CollidesWith.
 *
 * Setting a collider field also adds a default PhysicsBody when the entity has none. Collider
 * sizes and radii must be positive and finite, offsets finite; other values are rejected with
 * core::FieldValueOutOfRange. CollidesWith exposes its tags as `labels`, a list of strings.
 *
 * @note PhysicsBody's `type` is an int: 0 for static, 1 for dynamic. Other values are rejected
 * with core::FieldValueOutOfRange.
 * @throws core::DuplicateComponentName If one of them is already registered.
 */
ENGINE_PHYSICS_EXPORT void register_physics_fields(core::FieldRegistry& registry);

} // namespace engine::physics
