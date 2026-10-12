#pragma once

#include "engine/core/Vec2.hpp"

#include <cstdint>

namespace engine::physics {

/**
* @brief Physics properties of an entity.
*
* Registers an entity in the physics simulation. Entities with PhysicsBody
* are affected by forces and participate in collision detection/resolution.
*/
struct PhysicsBody {
    enum class Type : std::uint8_t {
        Static,   // Infinite mass, never moves (walls, terrain)
        Dynamic,  // Finite mass, affected by forces (players, projectiles)
        // TODO: add Kinematic type, moved only via velocity, unaffected by forces
    };

    Type type = Type::Static;

    core::Vec2 velocity{0.0f, 0.0f};
    core::Vec2 acceleration{0.0f, 0.0f};

    float mass = 1.0f;
    bool resolve_collisions = true;
    bool affected_by_gravity = true;

    std::uint32_t collision_layer = 1;
    std::uint32_t collision_mask = 0xFFFFFFFF;
};

/**
* @brief Axis-Aligned Bounding Box collider.
*
* Defines a rectangular area for collision detection. The offset is relative to
* the entity's position.
*/
struct AABBCollider {
    engine::core::Vec2 offset;
    engine::core::Vec2 size;
};

/**
* @brief Circular collider.
*
* Defines a circular area for collision detection. The offset is relative to
* the entity's position.
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
