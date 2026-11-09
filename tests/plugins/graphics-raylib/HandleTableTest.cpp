#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "HandleTable.hpp"
#include "engine/graphics/Types.hpp"

using engine::graphics::RenderTargetId;
using engine::graphics::ShaderId;
using engine::graphics::TextureId;

using TextureTable = HandleTable<TextureId, int>;

TEST(HandleTable, NeverHandsOutThePlaceholderId) {
    TextureTable table;
    EXPECT_NE(table.add(7), TextureId{});
}

TEST(HandleTable, GivesEachTextureItsOwnId) {
    TextureTable table;
    const TextureId first = table.add(1);
    const TextureId second = table.add(2);

    EXPECT_NE(first, second);
    ASSERT_NE(table.find(first), nullptr);
    ASSERT_NE(table.find(second), nullptr);
    EXPECT_EQ(*table.find(first), 1);
    EXPECT_EQ(*table.find(second), 2);
}

TEST(HandleTable, FindsNothingForUnknownIds) {
    TextureTable table;
    table.add(1);

    EXPECT_EQ(table.find(TextureId{}), nullptr);
    EXPECT_EQ(table.find(TextureId{.value = 42}), nullptr);
}

TEST(HandleTable, RemoveGivesTheHandleBackOnce) {
    TextureTable table;
    const TextureId id = table.add(5);

    EXPECT_EQ(table.remove(id), 5);
    EXPECT_EQ(table.find(id), nullptr);
    EXPECT_EQ(table.remove(id), std::nullopt);
}

TEST(HandleTable, RemovedIdIsNotReused) {
    TextureTable table;
    const TextureId removed = table.add(1);
    table.remove(removed);

    EXPECT_NE(table.add(2), removed);
}

TEST(HandleTable, RemoveAllEmptiesTheTable) {
    TextureTable table;
    const TextureId first = table.add(1);
    const TextureId second = table.add(2);

    auto handles = table.remove_all();
    std::ranges::sort(handles);

    EXPECT_EQ(handles, (std::vector<int>{1, 2}));
    EXPECT_EQ(table.find(first), nullptr);
    EXPECT_EQ(table.find(second), nullptr);
    EXPECT_TRUE(table.remove_all().empty());
}

TEST(HandleTable, KeepsTargetsAndShadersUnderTheirOwnIds) {
    HandleTable<RenderTargetId, int> targets;
    HandleTable<ShaderId, int> shaders;
    const RenderTargetId target = targets.add(3);
    const ShaderId shader = shaders.add(4);

    EXPECT_NE(target, RenderTargetId{});
    EXPECT_NE(shader, ShaderId{});
    ASSERT_NE(targets.find(target), nullptr);
    ASSERT_NE(shaders.find(shader), nullptr);
    EXPECT_EQ(*targets.find(target), 3);
    EXPECT_EQ(*shaders.find(shader), 4);
}

TEST(HandleTable, FindLetsTheHandleBeChanged) {
    TextureTable table;
    const TextureId id = table.add(1);

    *table.find(id) = 9;

    EXPECT_EQ(table.remove(id), 9);
}
