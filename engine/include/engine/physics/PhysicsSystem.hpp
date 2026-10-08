#pragma once

#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/physics/Export.hpp"

namespace engine::physics {

class ENGINE_PHYSICS_EXPORT PhysicsSystem {
public:
    PhysicsSystem(core::EntityManager& entity_manager, core::event::EventBus& event_bus);

    void update(float dt);
private:
    core::EntityManager& _entity_manager;
    core::event::EventBus& _event_bus;

    void integrate_velocity(float dt);
    void detect_and_handle_collisions();
};

} // namespace engine::physics