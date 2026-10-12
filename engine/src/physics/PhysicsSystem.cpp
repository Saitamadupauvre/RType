#include "engine/physics/PhysicsSystem.hpp"

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/physics/Collisions.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Events.hpp"

namespace engine::physics {

PhysicsSystem::PhysicsSystem(core::EntityManager& entity_manager, core::event::EventBus& event_bus)
    : _entity_manager(entity_manager), _event_bus(event_bus) {}

void PhysicsSystem::update(float dt) {
    apply_forces();
    integrate_acceleration(dt);
    integrate_velocity(dt);
    detect_and_handle_collisions();
}

void PhysicsSystem::set_gravity(core::Vec2 gravity) { _gravity = gravity; }

core::Vec2 PhysicsSystem::get_gravity() const { return _gravity; }

void PhysicsSystem::apply_forces() {
    _entity_manager.query<physics::PhysicsBody>().each(
        [this](core::Entity, physics::PhysicsBody& body) {
            if (body.type == physics::PhysicsBody::Type::Dynamic && body.affected_by_gravity) {
                body.acceleration += _gravity;
            }
        });
}

void PhysicsSystem::integrate_acceleration(float dt) {
    _entity_manager.query<physics::PhysicsBody>().each(
        [dt](core::Entity, physics::PhysicsBody& body) {
            if (body.type == physics::PhysicsBody::Type::Dynamic) {
                body.velocity += body.acceleration * dt;
                body.acceleration = {0.0f, 0.0f};
            }
        });
}

void PhysicsSystem::integrate_velocity(float dt) {
    _entity_manager.query<core::Position, physics::PhysicsBody>().each(
        [dt](core::Entity, core::Position& position, physics::PhysicsBody& body) {
            if (body.type == physics::PhysicsBody::Type::Dynamic) {
                position.pos += body.velocity * dt;
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

    // TODO: optimize with broad and narrow phases
    // TODO: handle high-velocity tunneling (swept collision detection or raycast)
    // TODO: add mass validation (assert mass > 0 in debug, clamp to epsilon in release)
    // TODO: handle resting contact (objects stacked shouldn't bounce indefinitely)
    // TODO: consider iterative resolution for chained/multiple collisions per frame
    // TODO: maybe make body elasticity a property of the PhysicsBody
    for (size_t i = 0; i < entities.size(); ++i) {
        for (size_t ii = i + 1; ii < entities.size(); ++ii) {

            core::Entity a = entities[i];
            core::Entity b = entities[ii];

            auto& body_a = _entity_manager.get<physics::PhysicsBody>(a);
            auto& body_b = _entity_manager.get<physics::PhysicsBody>(b);
            if (body_a.type == physics::PhysicsBody::Type::Static &&
                body_b.type == physics::PhysicsBody::Type::Static)
                continue;
            if ((body_a.collision_mask & body_b.collision_layer) == 0)
                continue;

            core::Vec2 normal;
            float penetration_depth;
            bool collided = any_collide(a, b, normal, penetration_depth);
            if (!collided)
                continue;

            if (_entity_manager.has<physics::Scriptable>(a) ||
                _entity_manager.has<physics::Scriptable>(b)) {
                CollisionEvent event{.entity_a = a,
                                     .entity_b = b,
                                     .normal = normal,
                                     .penetration_depth = penetration_depth};
                _event_bus.publish(event);
            }

            if (!(body_a.resolve_collisions && body_b.resolve_collisions))
                continue;

            resolve_collision(a, b, normal, penetration_depth);
        }
    }
}

void PhysicsSystem::resolve_collision(core::Entity a, core::Entity b, core::Vec2 normal,
                                      float penetration_depth) {
    auto& body_a = _entity_manager.get<physics::PhysicsBody>(a);
    auto& body_b = _entity_manager.get<physics::PhysicsBody>(b);

    core::Vec2 n = normal.normalize();
    float m_a = (body_a.type == physics::PhysicsBody::Type::Static)
                    ? std::numeric_limits<float>::infinity()
                    : body_a.mass;
    float m_b = (body_b.type == physics::PhysicsBody::Type::Static)
                    ? std::numeric_limits<float>::infinity()
                    : body_b.mass;

    core::Vec2 correction = n * penetration_depth / (1 / m_a + 1 / m_b);
    _entity_manager.get<core::Position>(a).pos -= correction / m_a;
    _entity_manager.get<core::Position>(b).pos += correction / m_b;

    float v_rel = dot(body_b.velocity - body_a.velocity, n);
    if (v_rel > 0)
        return;

    float e = 0.5f;
    float j = -(1 + e) * v_rel / (1 / m_a + 1 / m_b);

    body_a.velocity -= j * n / m_a;
    body_b.velocity += j * n / m_b;
}

bool PhysicsSystem::any_collide(core::Entity a, core::Entity b, core::Vec2& out_normal,
                                float& out_p_depth) {
    if (_entity_manager.has<physics::AABBCollider>(a) &&
        _entity_manager.has<physics::AABBCollider>(b)) {
        core::Vec2 pos_a = _entity_manager.get<core::Position>(a).pos +
                           _entity_manager.get<physics::AABBCollider>(a).offset;
        core::Vec2 size_a = _entity_manager.get<physics::AABBCollider>(a).size;
        core::Vec2 pos_b = _entity_manager.get<core::Position>(b).pos +
                           _entity_manager.get<physics::AABBCollider>(b).offset;
        core::Vec2 size_b = _entity_manager.get<physics::AABBCollider>(b).size;
        if (check_aabb_collision(pos_a, size_a, pos_b, size_b, out_normal, out_p_depth))
            return true;
    } else if (_entity_manager.has<physics::CircleCollider>(a) &&
               _entity_manager.has<physics::CircleCollider>(b)) {
        core::Vec2 pos_a = _entity_manager.get<core::Position>(a).pos +
                           _entity_manager.get<physics::CircleCollider>(a).offset;
        float radius_a = _entity_manager.get<physics::CircleCollider>(a).radius;
        core::Vec2 pos_b = _entity_manager.get<core::Position>(b).pos +
                           _entity_manager.get<physics::CircleCollider>(b).offset;
        float radius_b = _entity_manager.get<physics::CircleCollider>(b).radius;
        if (check_circle_collision(pos_a, radius_a, pos_b, radius_b, out_normal, out_p_depth))
            return true;
    } else if (_entity_manager.has<physics::AABBCollider>(a) &&
               _entity_manager.has<physics::CircleCollider>(b)) {
        core::Vec2 aabb_pos = _entity_manager.get<core::Position>(a).pos +
                              _entity_manager.get<physics::AABBCollider>(a).offset;
        core::Vec2 aabb_size = _entity_manager.get<physics::AABBCollider>(a).size;
        core::Vec2 circle_pos = _entity_manager.get<core::Position>(b).pos +
                                _entity_manager.get<physics::CircleCollider>(b).offset;
        float circle_radius = _entity_manager.get<physics::CircleCollider>(b).radius;
        if (check_aabb_circle_collision(aabb_pos, aabb_size, circle_pos, circle_radius, out_normal,
                                        out_p_depth))
            return true;
    } else if (_entity_manager.has<physics::CircleCollider>(a) &&
               _entity_manager.has<physics::AABBCollider>(b)) {
        core::Vec2 circle_pos = _entity_manager.get<core::Position>(a).pos +
                                _entity_manager.get<physics::CircleCollider>(a).offset;
        float circle_radius = _entity_manager.get<physics::CircleCollider>(a).radius;
        core::Vec2 aabb_pos = _entity_manager.get<core::Position>(b).pos +
                              _entity_manager.get<physics::AABBCollider>(b).offset;
        core::Vec2 aabb_size = _entity_manager.get<physics::AABBCollider>(b).size;
        if (check_aabb_circle_collision(aabb_pos, aabb_size, circle_pos, circle_radius, out_normal,
                                        out_p_depth))
            return true;
    }
    return false;
}

} // namespace engine::physics