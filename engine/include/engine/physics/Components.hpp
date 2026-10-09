#pragma once

#include "engine/core/Vec2.hpp"

#include <cstdint>

namespace engine::physics {

/**
 * @brief Physics properties of an entity.
 *
 * Determines how the entity behaves in collisions and physics simulation.
 */
struct PhysicsBody {
    enum class Type : std::uint8_t { Static, Dynamic };

    Type type = Type::Static;
    bool resolve_collisions = true;
    float mass = 1.0f;
    std::uint32_t collision_layer = 1;
    std::uint32_t collision_mask = 0xFFFFFFFF;
};

/**
 * @brief Axis-aligned bounding box collider.
 *
 * Defines a rectangular collision shape relative to the entity's position.
 */
struct AABBCollider {
    engine::core::Vec2 offset;
    engine::core::Vec2 size;
};

/**
 * @brief Circle collider.
 *
 * Defines a circular collision shape relative to the entity's position.
 */
struct CircleCollider {
    engine::core::Vec2 offset;
    float radius = 1.0f;
};

/**
 * @brief Marks an entity as scriptable for game logic.
 *
 * Entities with this component fire collision events to Lua scripts, allowing
 * custom logic (damage, pickups, triggers) on top of physics simulation.
 * If either entity in a collision has Scriptable, a CollisionEvent is emitted.
 */
struct Scriptable {};

} // namespace engine::physics
