#include "Collisions.hpp"

#include "engine/core/ComponentPhysics.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Events.hpp"

namespace rtype::server {

namespace {

template <typename Component> void register_if_missing(engine::core::EntityManager& registry) {
    if (!registry.is_registered<Component>()) {
        registry.register_component<Component>();
    }
}

engine::core::EntityManager& with_physics_components(engine::core::EntityManager& registry) {
    register_if_missing<engine::core::Position>(registry);
    register_if_missing<engine::physics::PhysicsBody>(registry);
    register_if_missing<engine::physics::AABBCollider>(registry);
    register_if_missing<engine::physics::CircleCollider>(registry);
    register_if_missing<engine::physics::Scriptable>(registry);
    return registry;
}

} // namespace

Collisions::Collisions(engine::core::EntityManager& registry, engine::core::event::EventBus& events,
                       engine::script::ScriptRuntime& runtime)
    : _events(events), _physics(with_physics_components(registry), events),
      _subscription(events.subscribe<engine::physics::CollisionEvent>(
          [&runtime](const engine::physics::CollisionEvent& collision) {
              if (collision.penetration_depth > 0.F) {
                  runtime.collide(collision.entity_a, collision.entity_b);
              }
          })) {}

Collisions::~Collisions() { _events.get().unsubscribe(_subscription); }

void Collisions::step(float dt) { _physics.update(dt); }

} // namespace rtype::server
