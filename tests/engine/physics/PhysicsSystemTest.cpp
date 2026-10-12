#include <gtest/gtest.h>

#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/physics/Components.hpp"
#include "engine/physics/Events.hpp"
#include "engine/physics/PhysicsSystem.hpp"

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::event::EventBus;
using engine::physics::AABBCollider;
using engine::physics::CircleCollider;
using engine::physics::CollisionEvent;
using engine::physics::PhysicsBody;
using engine::physics::PhysicsSystem;
using engine::physics::Scriptable;

TEST(PhysicsSystem, DetectsEntitiesWithColliders) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto aabb_entity = manager.spawn();
    manager.add(aabb_entity, Position{100.0f, 100.0f});
    manager.add(aabb_entity, PhysicsBody{});
    manager.add(aabb_entity, AABBCollider{{0.0f, 0.0f}, {32.0f, 32.0f}});

    auto circle_entity = manager.spawn();
    manager.add(circle_entity, Position{200.0f, 200.0f});
    manager.add(circle_entity, PhysicsBody{});
    manager.add(circle_entity, CircleCollider{{0.0f, 0.0f}, 16.0f});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
}

TEST(PhysicsSystem, IntegratesVelocity) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Dynamic;
    body.velocity = {10.0f, 20.0f};
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.update(1.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.pos.y, 20.0f);
}

TEST(PhysicsSystem, DoesNotMoveStaticBodies) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{100.0f, 100.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Static;
    body.velocity = {10.0f, 20.0f};
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.update(1.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.pos.y, 100.0f);
}

TEST(PhysicsSystem, PublishesCollisionEventForAABBCollision) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_a, AABBCollider{.offset={0.0f, 0.0f}, .size={10.0f, 10.0f}});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_b, AABBCollider{.offset={0.0f, 0.0f}, .size={10.0f, 10.0f}});
    manager.add(entity_b, Scriptable{});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE((events[0].entity_a == entity_a && events[0].entity_b == entity_b) ||
                (events[0].entity_a == entity_b && events[0].entity_b == entity_a));
    EXPECT_GT(events[0].penetration_depth, 0.0f);
}

TEST(PhysicsSystem, PublishesCollisionEventForCircleCollision) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_a, CircleCollider{{0.0f, 0.0f}, 5.0f});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{8.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_b, CircleCollider{{0.0f, 0.0f}, 5.0f});
    manager.add(entity_b, Scriptable{});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE((events[0].entity_a == entity_a && events[0].entity_b == entity_b) ||
                (events[0].entity_a == entity_b && events[0].entity_b == entity_a));
    EXPECT_FLOAT_EQ(events[0].penetration_depth, 2.0f);
}

TEST(PhysicsSystem, PublishesCollisionEventForAABBCircleCollision) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto aabb_entity = manager.spawn();
    manager.add(aabb_entity, Position{0.0f, 0.0f});
    manager.add(aabb_entity, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(aabb_entity, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(aabb_entity, Scriptable{});

    auto circle_entity = manager.spawn();
    manager.add(circle_entity, Position{7.0f, 0.0f});
    manager.add(circle_entity, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(circle_entity, CircleCollider{{0.0f, 0.0f}, 5.0f});
    manager.add(circle_entity, Scriptable{});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_TRUE((events[0].entity_a == aabb_entity && events[0].entity_b == circle_entity) ||
                (events[0].entity_a == circle_entity && events[0].entity_b == aabb_entity));
    EXPECT_FLOAT_EQ(events[0].penetration_depth, 3.0f);
}

TEST(PhysicsSystem, NoEventWhenNoScriptableComponent) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{.type=PhysicsBody::Type::Dynamic});
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    EXPECT_EQ(events.size(), 0u);
}

TEST(PhysicsSystem, NoEventWhenLayerMaskFilters) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    PhysicsBody body_a;
    body_a.type = PhysicsBody::Type::Dynamic;
    body_a.collision_layer = 0b01;
    body_a.collision_mask = 0b01;
    manager.add(entity_a, body_a);
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    PhysicsBody body_b;
    body_b.type = PhysicsBody::Type::Dynamic;
    body_b.collision_layer = 0b10;
    body_b.collision_mask = 0b10;
    manager.add(entity_b, body_b);
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(entity_b, Scriptable{});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    EXPECT_EQ(events.size(), 0u);
}

TEST(PhysicsSystem, AppliesGravityToDynamicBodies) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Dynamic;
    body.affected_by_gravity = true;
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.set_gravity({0.0f, 100.0f});
    system.update(1.0f);

    auto& updated_body = manager.get<PhysicsBody>(entity);
    EXPECT_FLOAT_EQ(updated_body.velocity.y, 100.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.y, 100.0f);
}

TEST(PhysicsSystem, DoesNotApplyGravityToStaticBodies) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Static;
    body.affected_by_gravity = true;
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.set_gravity({0.0f, 100.0f});
    system.update(1.0f);

    auto& updated_body = manager.get<PhysicsBody>(entity);
    EXPECT_FLOAT_EQ(updated_body.velocity.y, 0.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.y, 0.0f);
}

TEST(PhysicsSystem, RespectsAffectedByGravityFlag) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Dynamic;
    body.affected_by_gravity = false;
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.set_gravity({0.0f, 100.0f});
    system.update(1.0f);

    auto& updated_body = manager.get<PhysicsBody>(entity);
    EXPECT_FLOAT_EQ(updated_body.velocity.y, 0.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.y, 0.0f);
}

TEST(PhysicsSystem, IntegratesAccelerationToVelocity) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Dynamic;
    body.velocity = {0.0f, 0.0f};
    body.acceleration = {10.0f, 20.0f};
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.update(1.0f);

    auto& updated_body = manager.get<PhysicsBody>(entity);
    EXPECT_FLOAT_EQ(updated_body.velocity.x, 10.0f);
    EXPECT_FLOAT_EQ(updated_body.velocity.y, 20.0f);
    EXPECT_FLOAT_EQ(updated_body.acceleration.x, 0.0f);
    EXPECT_FLOAT_EQ(updated_body.acceleration.y, 0.0f);
}

TEST(PhysicsSystem, CombinesGravityAndAcceleration) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    PhysicsBody body;
    body.type = PhysicsBody::Type::Dynamic;
    body.velocity = {0.0f, 0.0f};
    body.acceleration = {50.0f, 0.0f};
    body.affected_by_gravity = true;
    manager.add(entity, body);

    PhysicsSystem system(manager, bus);
    system.set_gravity({0.0f, 100.0f});
    system.update(1.0f);

    auto& updated_body = manager.get<PhysicsBody>(entity);
    EXPECT_FLOAT_EQ(updated_body.velocity.x, 50.0f);
    EXPECT_FLOAT_EQ(updated_body.velocity.y, 100.0f);

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.x, 50.0f);
    EXPECT_FLOAT_EQ(pos.pos.y, 100.0f);
}

TEST(PhysicsSystem, ResolvesCollisionBetweenDynamicBodies) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    PhysicsBody body_a;
    body_a.type = PhysicsBody::Type::Dynamic;
    body_a.mass = 1.0f;
    body_a.velocity = {10.0f, 0.0f};
    body_a.resolve_collisions = true;
    manager.add(entity_a, body_a);
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    PhysicsBody body_b;
    body_b.type = PhysicsBody::Type::Dynamic;
    body_b.mass = 1.0f;
    body_b.velocity = {-10.0f, 0.0f};
    body_b.resolve_collisions = true;
    manager.add(entity_b, body_b);
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);

    auto& pos_a = manager.get<Position>(entity_a);
    auto& pos_b = manager.get<Position>(entity_b);
    EXPECT_LT(pos_a.pos.x, pos_b.pos.x);

    auto& vel_a = manager.get<PhysicsBody>(entity_a).velocity;
    auto& vel_b = manager.get<PhysicsBody>(entity_b).velocity;
    EXPECT_LT(vel_a.x, 10.0f);
    EXPECT_GT(vel_b.x, -10.0f);
}

TEST(PhysicsSystem, ResolvesCollisionWithStaticBody) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto wall = manager.spawn();
    manager.add(wall, Position{50.0f, 0.0f});
    PhysicsBody wall_body;
    wall_body.type = PhysicsBody::Type::Static;
    wall_body.resolve_collisions = true;
    manager.add(wall, wall_body);
    manager.add(wall, AABBCollider{{0.0f, 0.0f}, {10.0f, 100.0f}});

    auto ball = manager.spawn();
    manager.add(ball, Position{45.0f, 0.0f});
    PhysicsBody ball_body;
    ball_body.type = PhysicsBody::Type::Dynamic;
    ball_body.mass = 1.0f;
    ball_body.velocity = {10.0f, 0.0f};
    ball_body.resolve_collisions = true;
    manager.add(ball, ball_body);
    manager.add(ball, CircleCollider{{0.0f, 0.0f}, 5.0f});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);

    auto& wall_pos = manager.get<Position>(wall);
    EXPECT_FLOAT_EQ(wall_pos.pos.x, 50.0f);

    auto& ball_pos = manager.get<Position>(ball);
    EXPECT_LT(ball_pos.pos.x, 50.0f);

    auto& ball_vel = manager.get<PhysicsBody>(ball).velocity;
    EXPECT_LT(ball_vel.x, 10.0f);
}

TEST(PhysicsSystem, DoesNotResolveWhenResolveFlagFalse) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    PhysicsBody body_a;
    body_a.type = PhysicsBody::Type::Dynamic;
    body_a.velocity = {10.0f, 0.0f};
    body_a.resolve_collisions = false;
    manager.add(entity_a, body_a);
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    PhysicsBody body_b;
    body_b.type = PhysicsBody::Type::Dynamic;
    body_b.velocity = {0.0f, 0.0f};
    body_b.resolve_collisions = true;
    manager.add(entity_b, body_b);
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);

    auto& vel_a = manager.get<PhysicsBody>(entity_a).velocity;
    auto& vel_b = manager.get<PhysicsBody>(entity_b).velocity;
    EXPECT_FLOAT_EQ(vel_a.x, 10.0f);
    EXPECT_FLOAT_EQ(vel_b.x, 0.0f);
}

TEST(PhysicsSystem, ResolvesCollisionWithDifferentMasses) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto light = manager.spawn();
    manager.add(light, Position{0.0f, 0.0f});
    PhysicsBody light_body;
    light_body.type = PhysicsBody::Type::Dynamic;
    light_body.mass = 1.0f;
    light_body.velocity = {10.0f, 0.0f};
    light_body.resolve_collisions = true;
    manager.add(light, light_body);
    manager.add(light, CircleCollider{{0.0f, 0.0f}, 5.0f});

    auto heavy = manager.spawn();
    manager.add(heavy, Position{8.0f, 0.0f});
    PhysicsBody heavy_body;
    heavy_body.type = PhysicsBody::Type::Dynamic;
    heavy_body.mass = 10.0f;
    heavy_body.velocity = {0.0f, 0.0f};
    heavy_body.resolve_collisions = true;
    manager.add(heavy, heavy_body);
    manager.add(heavy, CircleCollider{{0.0f, 0.0f}, 5.0f});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);

    auto& light_vel = manager.get<PhysicsBody>(light).velocity;
    auto& heavy_vel = manager.get<PhysicsBody>(heavy).velocity;

    EXPECT_LT(light_vel.x, 10.0f);
    EXPECT_GT(heavy_vel.x, 0.0f);
    EXPECT_GT(std::abs(light_vel.x - 10.0f), std::abs(heavy_vel.x));
}

TEST(PhysicsSystem, HandlesMultipleSimultaneousCollisions) {
    EntityManager manager;
    EventBus bus;

    manager.register_component<Position>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto center = manager.spawn();
    manager.add(center, Position{50.0f, 50.0f});
    PhysicsBody center_body;
    center_body.type = PhysicsBody::Type::Dynamic;
    center_body.mass = 1.0f;
    center_body.velocity = {0.0f, 0.0f};
    center_body.resolve_collisions = true;
    manager.add(center, center_body);
    manager.add(center, CircleCollider{{0.0f, 0.0f}, 5.0f});

    auto left_wall = manager.spawn();
    manager.add(left_wall, Position{30.0f, 50.0f});
    PhysicsBody left_body;
    left_body.type = PhysicsBody::Type::Static;
    left_body.resolve_collisions = true;
    manager.add(left_wall, left_body);
    manager.add(left_wall, AABBCollider{{0.0f, 0.0f}, {5.0f, 100.0f}});

    auto right_wall = manager.spawn();
    manager.add(right_wall, Position{70.0f, 50.0f});
    PhysicsBody right_body;
    right_body.type = PhysicsBody::Type::Static;
    right_body.resolve_collisions = true;
    manager.add(right_wall, right_body);
    manager.add(right_wall, AABBCollider{{0.0f, 0.0f}, {5.0f, 100.0f}});

    auto top_wall = manager.spawn();
    manager.add(top_wall, Position{50.0f, 30.0f});
    PhysicsBody top_body;
    top_body.type = PhysicsBody::Type::Static;
    top_body.resolve_collisions = true;
    manager.add(top_wall, top_body);
    manager.add(top_wall, AABBCollider{{0.0f, 0.0f}, {100.0f, 5.0f}});

    auto bottom_wall = manager.spawn();
    manager.add(bottom_wall, Position{50.0f, 70.0f});
    PhysicsBody bottom_body;
    bottom_body.type = PhysicsBody::Type::Static;
    bottom_body.resolve_collisions = true;
    manager.add(bottom_wall, bottom_body);
    manager.add(bottom_wall, AABBCollider{{0.0f, 0.0f}, {100.0f, 5.0f}});

    PhysicsSystem system(manager, bus);
    system.update(0.016f);

    auto& center_pos = manager.get<Position>(center);
    auto& center_vel = manager.get<PhysicsBody>(center).velocity;

    EXPECT_GT(center_pos.pos.x, 35.0f);
    EXPECT_LT(center_pos.pos.x, 65.0f);
    EXPECT_GT(center_pos.pos.y, 35.0f);
    EXPECT_LT(center_pos.pos.y, 65.0f);

    EXPECT_TRUE(std::isfinite(center_vel.x));
    EXPECT_TRUE(std::isfinite(center_vel.y));
}
