#pragma once

#include <functional>

#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/event/Subscription.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace rtype::server {

class Collisions {
public:
    Collisions(engine::core::EntityManager& registry, engine::core::event::EventBus& events,
               engine::script::ScriptRuntime& runtime);
    ~Collisions();

    Collisions(const Collisions&) = delete;
    Collisions& operator=(const Collisions&) = delete;
    Collisions(Collisions&&) = delete;
    Collisions& operator=(Collisions&&) = delete;

    void step(float dt);

private:
    std::reference_wrapper<engine::core::event::EventBus> _events;
    engine::physics::PhysicsSystem _physics;
    engine::core::event::Subscription _subscription;
};

} // namespace rtype::server
