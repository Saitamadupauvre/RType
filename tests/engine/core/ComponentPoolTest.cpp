#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ComponentPool.hpp"

using engine::core::ComponentPool;
using engine::core::Entity;

namespace {

struct Position {
    int x;
    int y;

    bool operator==(const Position&) const = default;
};

} // namespace

TEST(ComponentPool, StartsEmpty) {
    const ComponentPool<int> pool;

    EXPECT_TRUE(pool.empty());
    EXPECT_EQ(pool.size(), 0U);
    EXPECT_FALSE(pool.contains(Entity{0, 0}));
}

TEST(ComponentPool, InsertStoresComponent) {
    ComponentPool<Position> pool;
    const Entity entity{4, 0};

    pool.insert(entity, Position{1, 2});

    EXPECT_TRUE(pool.contains(entity));
    EXPECT_EQ(pool.get(entity), (Position{1, 2}));
    EXPECT_EQ(pool.size(), 1U);
}

TEST(ComponentPool, EmplaceConstructsInPlace) {
    ComponentPool<Position> pool;
    const Entity entity{2, 0};

    Position& position = pool.emplace(entity, Position{5, 6});

    EXPECT_EQ(position, (Position{5, 6}));
    EXPECT_EQ(&position, &pool.get(entity));
}

TEST(ComponentPool, EmplaceForwardsConstructorArguments) {
    ComponentPool<std::string> pool;
    const Entity entity{1, 0};

    pool.emplace(entity, 3U, 'a');

    EXPECT_EQ(pool.get(entity), "aaa");
}

TEST(ComponentPool, InsertReplacesExistingComponent) {
    ComponentPool<int> pool;
    const Entity entity{3, 0};

    pool.insert(entity, 1);
    pool.insert(entity, 2);

    EXPECT_EQ(pool.get(entity), 2);
    EXPECT_EQ(pool.size(), 1U);
}

TEST(ComponentPool, GetAllowsMutation) {
    ComponentPool<int> pool;
    const Entity entity{0, 0};
    pool.insert(entity, 1);

    pool.get(entity) = 9;

    EXPECT_EQ(std::as_const(pool).get(entity), 9);
}

TEST(ComponentPool, GetThrowsWhenComponentIsMissing) {
    ComponentPool<int> pool;
    pool.insert(Entity{1, 0}, 1);

    EXPECT_THROW(static_cast<void>(pool.get(Entity{0, 0})), std::out_of_range);
    EXPECT_THROW(static_cast<void>(pool.get(Entity{99, 0})), std::out_of_range);
    EXPECT_THROW(static_cast<void>(std::as_const(pool).get(Entity{2, 0})), std::out_of_range);
}

TEST(ComponentPool, FindReturnsNullWhenComponentIsMissing) {
    ComponentPool<int> pool;
    const Entity entity{1, 0};
    pool.insert(entity, 7);

    ASSERT_NE(pool.find(entity), nullptr);
    EXPECT_EQ(*pool.find(entity), 7);
    EXPECT_EQ(pool.find(Entity{5, 0}), nullptr);
    EXPECT_EQ(std::as_const(pool).find(Entity{0, 0}), nullptr);
}

TEST(ComponentPool, EraseRemovesComponent) {
    ComponentPool<int> pool;
    const Entity entity{1, 0};
    pool.insert(entity, 1);

    EXPECT_TRUE(pool.erase(entity));
    EXPECT_FALSE(pool.contains(entity));
    EXPECT_TRUE(pool.empty());
}

TEST(ComponentPool, EraseMissingComponentReturnsFalse) {
    ComponentPool<int> pool;
    pool.insert(Entity{1, 0}, 1);

    EXPECT_FALSE(pool.erase(Entity{0, 0}));
    EXPECT_FALSE(pool.erase(Entity{50, 0}));
    EXPECT_EQ(pool.size(), 1U);
}

TEST(ComponentPool, EraseTwiceReturnsFalseTheSecondTime) {
    ComponentPool<int> pool;
    const Entity entity{1, 0};
    pool.insert(entity, 1);

    EXPECT_TRUE(pool.erase(entity));
    EXPECT_FALSE(pool.erase(entity));
}

TEST(ComponentPool, EraseKeepsDenseArrayPacked) {
    ComponentPool<int> pool;
    const Entity a{10, 0};
    const Entity b{20, 0};
    const Entity c{30, 0};
    pool.insert(a, 1);
    pool.insert(b, 2);
    pool.insert(c, 3);

    pool.erase(a);

    EXPECT_EQ(pool.size(), 2U);
    EXPECT_EQ(pool.get(b), 2);
    EXPECT_EQ(pool.get(c), 3);
    const std::vector<int> values(pool.begin(), pool.end());
    EXPECT_EQ(values, (std::vector<int>{3, 2}));
    EXPECT_EQ(pool.entities().front(), c);
}

TEST(ComponentPool, EraseLastComponentKeepsOthers) {
    ComponentPool<int> pool;
    const Entity a{1, 0};
    const Entity b{2, 0};
    pool.insert(a, 1);
    pool.insert(b, 2);

    pool.erase(b);

    EXPECT_EQ(pool.get(a), 1);
    EXPECT_EQ(pool.size(), 1U);
}

TEST(ComponentPool, ComponentCanBeReinsertedAfterErase) {
    ComponentPool<int> pool;
    const Entity a{1, 0};
    const Entity b{2, 0};
    pool.insert(a, 1);
    pool.insert(b, 2);
    pool.erase(a);

    pool.insert(a, 5);

    EXPECT_EQ(pool.get(a), 5);
    EXPECT_EQ(pool.get(b), 2);
    EXPECT_EQ(pool.size(), 2U);
}

TEST(ComponentPool, StaleHandleDoesNotSeeNewComponent) {
    ComponentPool<int> pool;
    const Entity stale{3, 0};
    const Entity fresh{3, 1};
    pool.insert(fresh, 8);

    EXPECT_FALSE(pool.contains(stale));
    EXPECT_EQ(pool.find(stale), nullptr);
    EXPECT_FALSE(pool.erase(stale));
    EXPECT_THROW(static_cast<void>(pool.get(stale)), std::out_of_range);
    EXPECT_EQ(pool.get(fresh), 8);
}

TEST(ComponentPool, NewGenerationReplacesStaleComponent) {
    ComponentPool<int> pool;
    const Entity stale{3, 0};
    const Entity fresh{3, 1};
    pool.insert(stale, 1);

    pool.insert(fresh, 2);

    EXPECT_EQ(pool.size(), 1U);
    EXPECT_FALSE(pool.contains(stale));
    EXPECT_EQ(pool.get(fresh), 2);
}

TEST(ComponentPool, IterationVisitsEveryComponentOnce) {
    ComponentPool<int> pool;
    for (std::uint32_t i = 0; i < 5; ++i) {
        pool.insert(Entity{i * 3, 0}, static_cast<int>(i));
    }

    int sum = 0;
    for (const int value : std::as_const(pool)) {
        sum += value;
    }

    EXPECT_EQ(sum, 10);
    EXPECT_EQ(pool.entities().size(), 5U);
}

TEST(ComponentPool, IterationAllowsMutation) {
    ComponentPool<int> pool;
    pool.insert(Entity{1, 0}, 1);
    pool.insert(Entity{2, 0}, 2);

    for (int& value : pool) {
        value *= 10;
    }

    EXPECT_EQ(pool.get(Entity{1, 0}), 10);
    EXPECT_EQ(pool.get(Entity{2, 0}), 20);
}

TEST(ComponentPool, ClearRemovesEverything) {
    ComponentPool<int> pool;
    const Entity entity{7, 0};
    pool.insert(entity, 1);

    pool.clear();

    EXPECT_TRUE(pool.empty());
    EXPECT_FALSE(pool.contains(entity));
    pool.insert(entity, 2);
    EXPECT_EQ(pool.get(entity), 2);
}

TEST(ComponentPool, SupportsMoveOnlyComponents) {
    ComponentPool<std::unique_ptr<int>> pool;
    const Entity a{1, 0};
    const Entity b{2, 0};
    pool.emplace(a, std::make_unique<int>(1));
    pool.emplace(b, std::make_unique<int>(2));

    pool.erase(a);

    EXPECT_EQ(*pool.get(b), 2);
}

TEST(ComponentPool, KeepsLookupsCorrectAfterManyRemovals) {
    ComponentPool<int> pool;
    constexpr std::uint32_t count = 200;
    for (std::uint32_t i = 0; i < count; ++i) {
        pool.insert(Entity{i, 0}, static_cast<int>(i));
    }
    for (std::uint32_t i = 0; i < count; i += 2) {
        ASSERT_TRUE(pool.erase(Entity{i, 0}));
    }

    EXPECT_EQ(pool.size(), count / 2);
    for (std::uint32_t i = 0; i < count; ++i) {
        const Entity entity{i, 0};
        if (i % 2 == 0) {
            EXPECT_FALSE(pool.contains(entity));
        } else {
            EXPECT_EQ(pool.get(entity), static_cast<int>(i));
        }
    }
}
