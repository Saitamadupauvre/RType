#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "engine/core/Entity.hpp"

using engine::core::Entity;

TEST(Entity, KeepsIndexAndGeneration) {
    const Entity entity{42, 7};

    EXPECT_EQ(entity.index(), 42U);
    EXPECT_EQ(entity.generation(), 7U);
}

TEST(Entity, KeepsMaximumValues) {
    constexpr auto max = std::numeric_limits<std::uint32_t>::max();
    const Entity entity{max, max};

    EXPECT_EQ(entity.index(), max);
    EXPECT_EQ(entity.generation(), max);
}

TEST(Entity, RoundTripsThroughId) {
    const Entity entity{3, 9};

    EXPECT_EQ(Entity{entity.id()}, entity);
}

TEST(Entity, DiffersWhenIndexOrGenerationDiffers) {
    EXPECT_NE((Entity{1, 0}), (Entity{1, 1}));
    EXPECT_NE((Entity{1, 0}), (Entity{2, 0}));
}
