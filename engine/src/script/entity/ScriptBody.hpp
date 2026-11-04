#pragma once

#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/physics/Components.hpp"

namespace engine::script {

void give_script_body(core::EntityManager& world, core::Entity entity);
physics::PhysicsBody& script_body(core::EntityManager& world, core::Entity entity);

} // namespace engine::script
