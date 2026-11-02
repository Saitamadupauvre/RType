#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;

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

TEST(ComponentPhysics, RegisterInEntityManager) {
    EntityManager manager;
    manager.register_component<Position>();

    auto entity = manager.spawn();
    EXPECT_TRUE(manager.alive(entity));

    manager.add(entity, Position{100.0f, 200.0f});

    auto& pos = manager.get<Position>(entity);
    EXPECT_FLOAT_EQ(pos.pos.x, 100.0f);
    EXPECT_FLOAT_EQ(pos.pos.y, 200.0f);
}
