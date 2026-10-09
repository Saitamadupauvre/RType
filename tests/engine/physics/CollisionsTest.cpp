#include <gtest/gtest.h>

#include "engine/core/Vec2.hpp"
#include "engine/physics/Collisions.hpp"

using engine::core::Vec2;
using namespace engine::physics;

TEST(AABB, DetectsOverlap) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f}, Vec2{5.0f, 0.0f},
                                         Vec2{10.0f, 10.0f}, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_FLOAT_EQ(normal.x, 1.0f);
    EXPECT_FLOAT_EQ(normal.y, 0.0f);
    EXPECT_FLOAT_EQ(depth, 5.0f);
}

TEST(AABB, NoOverlap) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f}, Vec2{20.0f, 0.0f},
                                         Vec2{10.0f, 10.0f}, normal, depth);

    EXPECT_FALSE(collided);
}

TEST(AABB, EdgeTouching) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f}, Vec2{10.0f, 0.0f},
                                         Vec2{10.0f, 10.0f}, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_NEAR(depth, 0.0f, 0.001f);
}

TEST(Circle, DetectsOverlap) {
    Vec2 normal;
    float depth;

    bool collided =
        check_circle_collision(Vec2{0.0f, 0.0f}, 5.0f, Vec2{8.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_FLOAT_EQ(normal.x, 1.0f);
    EXPECT_FLOAT_EQ(normal.y, 0.0f);
    EXPECT_FLOAT_EQ(depth, 2.0f);
}

TEST(Circle, NoOverlap) {
    Vec2 normal;
    float depth;

    bool collided =
        check_circle_collision(Vec2{0.0f, 0.0f}, 5.0f, Vec2{20.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_FALSE(collided);
}

TEST(Circle, ExactlyTouching) {
    Vec2 normal;
    float depth;

    bool collided =
        check_circle_collision(Vec2{0.0f, 0.0f}, 5.0f, Vec2{10.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_NEAR(depth, 0.0f, 0.001f);
}

TEST(Circle, Overlapping) {
    Vec2 normal;
    float depth;

    bool collided =
        check_circle_collision(Vec2{0.0f, 0.0f}, 5.0f, Vec2{0.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_FLOAT_EQ(depth, 10.0f);
}

TEST(AABBCircle, CircleTouchingEdge) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_circle_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f},
                                                Vec2{7.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_FLOAT_EQ(normal.x, 1.0f);
    EXPECT_FLOAT_EQ(normal.y, 0.0f);
    EXPECT_FLOAT_EQ(depth, 3.0f);
}

TEST(AABBCircle, CircleTouchingCorner) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_circle_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f},
                                                Vec2{8.0f, 8.0f}, 5.0f, normal, depth);

    EXPECT_TRUE(collided);
    float expected_normal_x = 1.0f / std::sqrt(2.0f);
    float expected_normal_y = 1.0f / std::sqrt(2.0f);
    EXPECT_NEAR(normal.x, expected_normal_x, 0.01f);
    EXPECT_NEAR(normal.y, expected_normal_y, 0.01f);
}

TEST(AABBCircle, NoOverlap) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_circle_collision(Vec2{0.0f, 0.0f}, Vec2{10.0f, 10.0f},
                                                Vec2{20.0f, 0.0f}, 5.0f, normal, depth);

    EXPECT_FALSE(collided);
}

TEST(AABBCircle, CircleInsideAABB) {
    Vec2 normal;
    float depth;

    bool collided = check_aabb_circle_collision(Vec2{0.0f, 0.0f}, Vec2{20.0f, 20.0f},
                                                Vec2{0.0f, 0.0f}, 3.0f, normal, depth);

    EXPECT_TRUE(collided);
    EXPECT_GT(depth, 0.0f);
}
