#include "engine/physics/PhysicsSystem.hpp"

#include <iostream>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/physics/Collisions.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Events.hpp"

namespace engine::physics {

PhysicsSystem::PhysicsSystem(core::EntityManager& entity_manager, core::event::EventBus& event_bus)
    : _entity_manager(entity_manager), _event_bus(event_bus) {}

void PhysicsSystem::update(float dt) {
    integrate_velocity(dt);
    detect_and_handle_collisions();
}

void PhysicsSystem::integrate_velocity(float dt) {
    _entity_manager.query<core::Position, core::Velocity, physics::PhysicsBody>().each(
        [dt](core::Entity, core::Position& position, core::Velocity& velocity,
             physics::PhysicsBody& body) {
            if (body.type == physics::PhysicsBody::Type::Dynamic) {
                position.pos = position.pos + velocity.vel * dt;
            }
        });
}

void PhysicsSystem::detect_and_handle_collisions() {
    std::vector<core::Entity> entities;
    _entity_manager.query<core::Position, physics::PhysicsBody, physics::AABBCollider>().each(
        [&](core::Entity e, core::Position&, physics::PhysicsBody&, physics::AABBCollider&) {
            entities.push_back(e);
        });
    _entity_manager.query<core::Position, physics::PhysicsBody, physics::CircleCollider>().each(
        [&](core::Entity e, core::Position&, physics::PhysicsBody&, physics::CircleCollider&) {
            entities.push_back(e);
        });

    std::cerr << "[PhysicsSystem] Checking " << entities.size() << " entities with colliders\n";

    // TODO: optimize with broad and narrow phases
    // TODO: handle collision resolution when needed
    for (size_t i = 0; i < entities.size(); ++i) {
        for (size_t j = i + 1; j < entities.size(); ++j) {

            core::Entity a = entities[i];
            core::Entity b = entities[j];

            auto& body_a = _entity_manager.get<physics::PhysicsBody>(a);
            auto& body_b = _entity_manager.get<physics::PhysicsBody>(b);
            bool mask_check = (body_a.collision_mask & body_b.collision_layer) != 0;
            std::cerr << "  Pair [" << i << "," << j << "]: mask=" << mask_check << "\n";
            if (!mask_check)
                continue;

            bool collided = false;
            core::Vec2 normal;
            float penetration_depth;
            if (_entity_manager.has<physics::AABBCollider>(a) &&
                _entity_manager.has<physics::AABBCollider>(b)) {
                core::Vec2 pos_a = _entity_manager.get<core::Position>(a).pos +
                                   _entity_manager.get<physics::AABBCollider>(a).offset;
                core::Vec2 size_a = _entity_manager.get<physics::AABBCollider>(a).size;
                core::Vec2 pos_b = _entity_manager.get<core::Position>(b).pos +
                                   _entity_manager.get<physics::AABBCollider>(b).offset;
                core::Vec2 size_b = _entity_manager.get<physics::AABBCollider>(b).size;
                if (check_aabb_collision(pos_a, size_a, pos_b, size_b, normal, penetration_depth))
                    collided = true;
            } else if (_entity_manager.has<physics::CircleCollider>(a) &&
                       _entity_manager.has<physics::CircleCollider>(b)) {
                core::Vec2 pos_a = _entity_manager.get<core::Position>(a).pos +
                                   _entity_manager.get<physics::CircleCollider>(a).offset;
                float radius_a = _entity_manager.get<physics::CircleCollider>(a).radius;
                core::Vec2 pos_b = _entity_manager.get<core::Position>(b).pos +
                                   _entity_manager.get<physics::CircleCollider>(b).offset;
                float radius_b = _entity_manager.get<physics::CircleCollider>(b).radius;
                if (check_circle_collision(pos_a, radius_a, pos_b, radius_b, normal,
                                           penetration_depth))
                    collided = true;
            } else if (_entity_manager.has<physics::AABBCollider>(a) &&
                       _entity_manager.has<physics::CircleCollider>(b)) {
                core::Vec2 aabb_pos = _entity_manager.get<core::Position>(a).pos +
                                      _entity_manager.get<physics::AABBCollider>(a).offset;
                core::Vec2 aabb_size = _entity_manager.get<physics::AABBCollider>(a).size;
                core::Vec2 circle_pos = _entity_manager.get<core::Position>(b).pos +
                                        _entity_manager.get<physics::CircleCollider>(b).offset;
                float circle_radius = _entity_manager.get<physics::CircleCollider>(b).radius;
                if (check_aabb_circle_collision(aabb_pos, aabb_size, circle_pos, circle_radius,
                                                normal, penetration_depth))
                    collided = true;
            } else if (_entity_manager.has<physics::CircleCollider>(a) &&
                       _entity_manager.has<physics::AABBCollider>(b)) {
                core::Vec2 circle_pos = _entity_manager.get<core::Position>(a).pos +
                                        _entity_manager.get<physics::CircleCollider>(a).offset;
                float circle_radius = _entity_manager.get<physics::CircleCollider>(a).radius;
                core::Vec2 aabb_pos = _entity_manager.get<core::Position>(b).pos +
                                      _entity_manager.get<physics::AABBCollider>(b).offset;
                core::Vec2 aabb_size = _entity_manager.get<physics::AABBCollider>(b).size;
                if (check_aabb_circle_collision(aabb_pos, aabb_size, circle_pos, circle_radius,
                                                normal, penetration_depth))
                    collided = true;
            }

            bool has_scriptable = _entity_manager.has<physics::Scriptable>(a) ||
                                  _entity_manager.has<physics::Scriptable>(b);
            std::cerr << "    collided=" << collided << " scriptable=" << has_scriptable << "\n";
            if (collided && has_scriptable) {
                std::cerr << "    Publishing event\n";
                CollisionEvent event{.entity_a = a,
                                     .entity_b = b,
                                     .normal = normal,
                                     .penetration_depth = penetration_depth};
                _event_bus.publish(event);
            }
        }
    }
}

} // namespace engine::physics