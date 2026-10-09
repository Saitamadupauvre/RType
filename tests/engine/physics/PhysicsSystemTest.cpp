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
using engine::core::Velocity;
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
    manager.register_component<Velocity>();
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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{0.0f, 0.0f});
    manager.add(entity, Velocity{10.0f, 20.0f});
    manager.add(entity, PhysicsBody{PhysicsBody::Type::Dynamic, true, 1.0f, 1, 0xFFFFFFFF});

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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity = manager.spawn();
    manager.add(entity, Position{100.0f, 100.0f});
    manager.add(entity, Velocity{10.0f, 20.0f});
    manager.add(entity, PhysicsBody{PhysicsBody::Type::Static, true, 1.0f, 1, 0xFFFFFFFF});

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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{});
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{});
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{});
    manager.add(entity_a, CircleCollider{{0.0f, 0.0f}, 5.0f});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{8.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{});
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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto aabb_entity = manager.spawn();
    manager.add(aabb_entity, Position{0.0f, 0.0f});
    manager.add(aabb_entity, PhysicsBody{});
    manager.add(aabb_entity, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(aabb_entity, Scriptable{});

    auto circle_entity = manager.spawn();
    manager.add(circle_entity, Position{7.0f, 0.0f});
    manager.add(circle_entity, PhysicsBody{});
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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{});
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{});
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
    manager.register_component<Velocity>();
    manager.register_component<PhysicsBody>();
    manager.register_component<AABBCollider>();
    manager.register_component<CircleCollider>();
    manager.register_component<Scriptable>();

    auto entity_a = manager.spawn();
    manager.add(entity_a, Position{0.0f, 0.0f});
    manager.add(entity_a, PhysicsBody{PhysicsBody::Type::Dynamic, true, 1.0f, 0b01, 0b01});
    manager.add(entity_a, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(entity_a, Scriptable{});

    auto entity_b = manager.spawn();
    manager.add(entity_b, Position{5.0f, 0.0f});
    manager.add(entity_b, PhysicsBody{PhysicsBody::Type::Dynamic, true, 1.0f, 0b10, 0b10});
    manager.add(entity_b, AABBCollider{{0.0f, 0.0f}, {10.0f, 10.0f}});
    manager.add(entity_b, Scriptable{});

    std::vector<CollisionEvent> events;
    bus.subscribe<CollisionEvent>([&events](const CollisionEvent& evt) { events.push_back(evt); });

    PhysicsSystem system(manager, bus);
    system.update(0.016f);
    bus.dispatch();

    EXPECT_EQ(events.size(), 0u);
}

TEST(PhysicsBody, DefaultMaskAllowsCollision) {
    PhysicsBody a{};
    PhysicsBody b{};
    EXPECT_NE(0u, a.collision_mask & b.collision_layer);
    EXPECT_NE(0u, b.collision_mask & a.collision_layer);
}
