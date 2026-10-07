#include <gtest/gtest.h>

#include <cmath>
#include <sstream>

#include "engine/core/Vec2.hpp"

using engine::core::Vec2;

TEST(Vec2, DefaultConstructor) {
    Vec2 v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
}

TEST(Vec2, ParameterizedConstructor) {
    Vec2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.x, 3.0f);
    EXPECT_FLOAT_EQ(v.y, 4.0f);
}

TEST(Vec2, Addition) {
    Vec2 a(1.0f, 2.0f);
    Vec2 b(3.0f, 4.0f);
    Vec2 result = a + b;
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}

TEST(Vec2, Subtraction) {
    Vec2 a(5.0f, 7.0f);
    Vec2 b(2.0f, 3.0f);
    Vec2 result = a - b;
    EXPECT_FLOAT_EQ(result.x, 3.0f);
    EXPECT_FLOAT_EQ(result.y, 4.0f);
}

TEST(Vec2, ScalarMultiplication) {
    Vec2 v(2.0f, 3.0f);
    Vec2 result = v * 2.0f;
    EXPECT_FLOAT_EQ(result.x, 4.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}

TEST(Vec2, Length) {
    Vec2 v(3.0f, 4.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
}

TEST(Vec2, LengthZeroVector) {
    Vec2 v(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(v.length(), 0.0f);
}

TEST(Vec2, Normalize) {
    Vec2 v(3.0f, 4.0f);
    Vec2 result = v.normalize();
    EXPECT_FLOAT_EQ(result.x, 0.6f);
    EXPECT_FLOAT_EQ(result.y, 0.8f);
    EXPECT_NEAR(result.length(), 1.0f, 1e-6f);
}

TEST(Vec2, NormalizeZeroVector) {
    Vec2 v(0.0f, 0.0f);
    Vec2 result = v.normalize();
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.0f);
}

TEST(Vec2, OutputOperator) {
    Vec2 v(1.5f, 2.5f);
    std::ostringstream oss;
    oss << v;
    EXPECT_EQ(oss.str(), "Vec2(1.5, 2.5)");
}

TEST(Vec2, NegativeValues) {
    Vec2 v(-3.0f, -4.0f);
    EXPECT_FLOAT_EQ(v.x, -3.0f);
    EXPECT_FLOAT_EQ(v.y, -4.0f);
    EXPECT_FLOAT_EQ(v.length(), 5.0f);
}

TEST(Vec2, ChainedOperations) {
    Vec2 a(1.0f, 2.0f);
    Vec2 b(3.0f, 4.0f);
    Vec2 c(5.0f, 6.0f);
    Vec2 result = (a + b) * 2.0f - c;
    EXPECT_FLOAT_EQ(result.x, 3.0f);
    EXPECT_FLOAT_EQ(result.y, 6.0f);
}
