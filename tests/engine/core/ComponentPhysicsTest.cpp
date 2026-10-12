#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::core::Velocity;

TEST(Position, DefaultConstructor) {
    Position p;
    EXPECT_FLOAT_EQ(p.pos.x, 0.0f);
    EXPECT_FLOAT_EQ(p.pos.y, 0.0f);
}

TEST(Position, Vec2Constructor) {
    Position p(Vec2{10.0f, 20.0f});
    EXPECT_FLOAT_EQ(p.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(p.pos.y, 20.0f);
}

TEST(Position, FloatConstructor) {
    Position p(5.0f, 15.0f);
    EXPECT_FLOAT_EQ(p.pos.x, 5.0f);
    EXPECT_FLOAT_EQ(p.pos.y, 15.0f);
}

TEST(Position, DirectFieldAccess) {
    Position p(10.0f, 20.0f);
    p.pos.x += 5.0f;
    p.pos.y -= 3.0f;
    EXPECT_FLOAT_EQ(p.pos.x, 15.0f);
    EXPECT_FLOAT_EQ(p.pos.y, 17.0f);
}

TEST(Velocity, DefaultConstructor) {
    Velocity v;
    EXPECT_FLOAT_EQ(v.vel.x, 0.0f);
    EXPECT_FLOAT_EQ(v.vel.y, 0.0f);
}

TEST(Velocity, Vec2Constructor) {
    Velocity v(Vec2{100.0f, 200.0f});
    EXPECT_FLOAT_EQ(v.vel.x, 100.0f);
    EXPECT_FLOAT_EQ(v.vel.y, 200.0f);
}

TEST(Velocity, FloatConstructor) {
    Velocity v(50.0f, 150.0f);
    EXPECT_FLOAT_EQ(v.vel.x, 50.0f);
    EXPECT_FLOAT_EQ(v.vel.y, 150.0f);
}

TEST(Velocity, DirectFieldAccess) {
    Velocity v(100.0f, 200.0f);
    v.vel.x *= 0.5f;
    v.vel.y *= 0.5f;
    EXPECT_FLOAT_EQ(v.vel.x, 50.0f);
    EXPECT_FLOAT_EQ(v.vel.y, 100.0f);
}

TEST(ComponentPhysics, RegisterInEntityManager) {
    EntityManager manager;
    manager.register_component<Position>();
    manager.register_component<Velocity>();

    auto entity = manager.spawn();
    EXPECT_TRUE(manager.alive(entity));

    manager.add(entity, Position{100.0f, 200.0f});
    manager.add(entity, Velocity{10.0f, 20.0f});

    auto& pos = manager.get<Position>(entity);
    auto& vel = manager.get<Velocity>(entity);

    EXPECT_FLOAT_EQ(pos.pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.pos.y, 200.0f);
    EXPECT_FLOAT_EQ(vel.vel.x, 10.0f);
    EXPECT_FLOAT_EQ(vel.vel.y, 20.0f);
}

TEST(ComponentPhysics, ViewPositionAndVelocity) {
    EntityManager manager;
    manager.register_component<Position>();
    manager.register_component<Velocity>();

    auto entity1 = manager.spawn();
    auto entity2 = manager.spawn();
    auto entity3 = manager.spawn();

    manager.add(entity1, Position{0.0f, 0.0f});
    manager.add(entity1, Velocity{10.0f, 10.0f});

    manager.add(entity2, Position{100.0f, 100.0f});
    manager.add(entity2, Velocity{20.0f, 20.0f});

    manager.add(entity3, Position{200.0f, 200.0f});

    int count = 0;
    manager.query<Position, Velocity>().each([&](auto, Position& pos, Velocity& vel) {
        pos.pos = pos.pos + vel.vel;
        ++count;
    });

    EXPECT_EQ(count, 2);

    auto& pos1 = manager.get<Position>(entity1);
    auto& pos2 = manager.get<Position>(entity2);
    auto& pos3 = manager.get<Position>(entity3);

    EXPECT_FLOAT_EQ(pos1.pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos1.pos.y, 10.0f);
    EXPECT_FLOAT_EQ(pos2.pos.x, 120.0f);
    EXPECT_FLOAT_EQ(pos2.pos.y, 120.0f);
    EXPECT_FLOAT_EQ(pos3.pos.x, 200.0f);
    EXPECT_FLOAT_EQ(pos3.pos.y, 200.0f);
}
