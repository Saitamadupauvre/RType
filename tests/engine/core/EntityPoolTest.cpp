#include <gtest/gtest.h>

#include "engine/core/EntityPool.hpp"

using engine::core::Entity;
using engine::core::EntityPool;

TEST(EntityPool, CreatesDistinctLivingEntities) {
    EntityPool pool;

    const Entity first = pool.create();
    const Entity second = pool.create();

    EXPECT_NE(first, second);
    EXPECT_TRUE(pool.alive(first));
    EXPECT_TRUE(pool.alive(second));
    EXPECT_EQ(pool.size(), 2U);
}

TEST(EntityPool, DestroyedEntityIsNotAlive) {
    EntityPool pool;
    const Entity entity = pool.create();

    EXPECT_TRUE(pool.destroy(entity));
    EXPECT_FALSE(pool.alive(entity));
    EXPECT_EQ(pool.size(), 0U);
}

TEST(EntityPool, ReusesDestroyedIndexWithNewGeneration) {
    EntityPool pool;
    const Entity old_entity = pool.create();
    pool.destroy(old_entity);

    const Entity new_entity = pool.create();

    EXPECT_EQ(new_entity.index(), old_entity.index());
    EXPECT_NE(new_entity.generation(), old_entity.generation());
}

TEST(EntityPool, StaleHandleIsNotAlive) {
    EntityPool pool;
    const Entity stale = pool.create();
    pool.destroy(stale);
    const Entity fresh = pool.create();

    EXPECT_FALSE(pool.alive(stale));
    EXPECT_TRUE(pool.alive(fresh));
}

TEST(EntityPool, DestroyingStaleHandleKeepsNewEntity) {
    EntityPool pool;
    const Entity stale = pool.create();
    pool.destroy(stale);
    const Entity fresh = pool.create();

    EXPECT_FALSE(pool.destroy(stale));
    EXPECT_TRUE(pool.alive(fresh));
}

TEST(EntityPool, DestroyingTwiceIsRejected) {
    EntityPool pool;
    const Entity entity = pool.create();
    pool.destroy(entity);

    EXPECT_FALSE(pool.destroy(entity));
    EXPECT_EQ(pool.size(), 0U);
}

TEST(EntityPool, DestroyingTwiceDoesNotDuplicateFreeIndex) {
    EntityPool pool;
    const Entity entity = pool.create();
    pool.destroy(entity);
    pool.destroy(entity);

    const Entity first = pool.create();
    const Entity second = pool.create();

    EXPECT_NE(first.index(), second.index());
}

TEST(EntityPool, UnknownHandleIsRejected) {
    EntityPool pool;
    pool.create();
    const Entity unknown{1000, 0};

    EXPECT_FALSE(pool.alive(unknown));
    EXPECT_FALSE(pool.destroy(unknown));
    EXPECT_EQ(pool.size(), 1U);
}

TEST(EntityPool, EmptyPoolHasNoLivingEntity) {
    const EntityPool pool;

    EXPECT_FALSE(pool.alive(Entity{0, 0}));
    EXPECT_EQ(pool.size(), 0U);
}
