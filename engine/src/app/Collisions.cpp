#include "Collisions.hpp"

#include "engine/core/ComponentPhysics.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Events.hpp"

namespace engine::app {

namespace {

template <typename Component> void register_if_missing(core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

core::EntityManager& with_physics_components(core::EntityManager& registry) {
    register_if_missing<core::Position>(registry);
    register_if_missing<physics::PhysicsBody>(registry);
    register_if_missing<physics::AABBCollider>(registry);
    register_if_missing<physics::CircleCollider>(registry);
    register_if_missing<physics::Scriptable>(registry);
    return registry;
}

} // namespace

Collisions::Collisions(core::EntityManager& registry, core::event::EventBus& events,
                       script::ScriptRuntime& runtime)
    : _events(events), _physics(with_physics_components(registry), events),
      _subscription(events.subscribe<physics::CollisionEvent>(
          [&runtime](const physics::CollisionEvent& collision) {
              if (collision.penetration_depth > 0.F) {
                  runtime.collide(collision.entity_a, collision.entity_b);
              }
          })) {}

Collisions::~Collisions() { _events.get().unsubscribe(_subscription); }

void Collisions::step(float dt) { _physics.update(dt); }

} // namespace engine::app
