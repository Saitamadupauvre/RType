#pragma once

#include "engine/core/Entity.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::physics {

/**
 * @brief Published when two entities collide.
 *
 * Fired if either entity has the Scriptable component, allowing Lua scripts to
 * handle game-specific collision logic (damage, pickups, triggers).
 *
 * The normal vector points from entity_a toward entity_b.
 * Penetration depth indicates how much the shapes overlap.
 */
struct CollisionEvent {
    engine::core::Entity entity_a;
    engine::core::Entity entity_b;
    engine::core::Vec2 normal;
    float penetration_depth;
};

} // namespace engine::physics
