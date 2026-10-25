#pragma once

#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/physics/Export.hpp"

namespace engine::physics {

class ENGINE_PHYSICS_EXPORT PhysicsSystem {
public:
    PhysicsSystem(core::EntityManager& entity_manager, core::event::EventBus& event_bus);

    void update(float dt);

    /* Really just sets a universal force upon a vector for dynamic bodies, called gravity
     * because... sugar */
    void set_gravity(core::Vec2 gravity);
    [[nodiscard]] core::Vec2 get_gravity() const;

private:
    core::EntityManager& _entity_manager;
    core::event::EventBus& _event_bus;
    core::Vec2 _gravity{0.0f, 0.0f};

    void apply_forces();
    void integrate_acceleration(float dt);
    void integrate_velocity(float dt);
    void detect_and_handle_collisions();

    bool any_collide(core::Entity a, core::Entity b, core::Vec2& out_normal, float& out_p_depth);

    /**
     * @brief Resolves collision using impulse-based physics.
     *
     * Performs position correction (depenetration) and velocity adjustment (impulse)
     * based on collision normal, penetration depth, and body masses.
     *
     * Static bodies (infinite mass) are treated via IEEE 754: 1/∞ = 0, so all
     * correction/impulse applies to the dynamic body only.
     *
     * @param a First entity
     * @param b Second entity
     * @param normal Collision normal (points from A toward B)
     * @param penetration_depth How much bodies overlap
     *
     * @note Uses coefficient of restitution e = 0.5 (semi-elastic)
     * @note See: https://www.chrishecker.com/Rigid_Body_Dynamics
     */
    void resolve_collision(core::Entity a, core::Entity b, core::Vec2 normal,
                           float penetration_depth);
};

} // namespace engine::physics