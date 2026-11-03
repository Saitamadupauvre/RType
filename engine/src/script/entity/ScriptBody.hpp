#pragma once

#include "engine/core/Entity.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/physics/Components.hpp"

namespace engine::script {

void give_script_body(core::EntityManager& world, core::Entity entity);
physics::PhysicsBody& script_body(core::EntityManager& world, core::Entity entity);

} // namespace engine::script
