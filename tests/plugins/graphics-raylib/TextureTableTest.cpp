#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "TextureTable.hpp"

using engine::graphics::TextureId;

TEST(TextureTable, NeverHandsOutThePlaceholderId) {
    TextureTable<int> table;
    EXPECT_NE(table.add(7), TextureId{});
}

TEST(TextureTable, GivesEachTextureItsOwnId) {
    TextureTable<int> table;
    const TextureId first = table.add(1);
    const TextureId second = table.add(2);

    EXPECT_NE(first, second);
    ASSERT_NE(table.find(first), nullptr);
    ASSERT_NE(table.find(second), nullptr);
    EXPECT_EQ(*table.find(first), 1);
    EXPECT_EQ(*table.find(second), 2);
}

TEST(TextureTable, FindsNothingForUnknownIds) {
    TextureTable<int> table;
    table.add(1);

    EXPECT_EQ(table.find(TextureId{}), nullptr);
    EXPECT_EQ(table.find(TextureId{.value = 42}), nullptr);
}

TEST(TextureTable, RemoveGivesTheHandleBackOnce) {
    TextureTable<int> table;
    const TextureId id = table.add(5);

    EXPECT_EQ(table.remove(id), 5);
    EXPECT_EQ(table.find(id), nullptr);
    EXPECT_EQ(table.remove(id), std::nullopt);
}

TEST(TextureTable, RemovedIdIsNotReused) {
    TextureTable<int> table;
    const TextureId removed = table.add(1);
    table.remove(removed);

    EXPECT_NE(table.add(2), removed);
}

TEST(TextureTable, RemoveAllEmptiesTheTable) {
    TextureTable<int> table;
    const TextureId first = table.add(1);
    const TextureId second = table.add(2);

    auto handles = table.remove_all();
    std::ranges::sort(handles);

    EXPECT_EQ(handles, (std::vector<int>{1, 2}));
    EXPECT_EQ(table.find(first), nullptr);
    EXPECT_EQ(table.find(second), nullptr);
    EXPECT_TRUE(table.remove_all().empty());
}
