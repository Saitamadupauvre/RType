#pragma once

#include <functional>

#include "engine/app/Export.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/event/Subscription.hpp"
#include "engine/physics/PhysicsSystem.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT Collisions {
public:
    Collisions(core::EntityManager& registry, core::event::EventBus& events,
               script::ScriptRuntime& runtime);
    ~Collisions();

    Collisions(const Collisions&) = delete;
    Collisions& operator=(const Collisions&) = delete;
    Collisions(Collisions&&) = delete;
    Collisions& operator=(Collisions&&) = delete;

    void step(float dt);

private:
    std::reference_wrapper<core::event::EventBus> _events;
    physics::PhysicsSystem _physics;
    core::event::Subscription _subscription;
};

} // namespace engine::app
