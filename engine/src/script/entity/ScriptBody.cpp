#include "entity/ScriptBody.hpp"

namespace engine::script {

void give_script_body(core::EntityManager& world, core::Entity entity) {
    auto& body = world.get<physics::PhysicsBody>(entity);
    body.type = physics::PhysicsBody::Type::Dynamic;
    body.resolve_collisions = false;
    body.affected_by_gravity = false;
}

physics::PhysicsBody& script_body(core::EntityManager& world, core::Entity entity) {
    if (!world.has<physics::PhysicsBody>(entity)) {
        world.add(entity, physics::PhysicsBody{});
        give_script_body(world, entity);
    }
    return world.get<physics::PhysicsBody>(entity);
}

} // namespace engine::script
