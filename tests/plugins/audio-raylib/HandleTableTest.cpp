#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "HandleTable.hpp"
#include "engine/audio/Types.hpp"

using engine::audio::MusicId;
using engine::audio::SoundId;

TEST(HandleTable, NeverHandsOutTheDefaultId) {
    HandleTable<SoundId, int> table;
    EXPECT_NE(table.add(7), SoundId{});
}

TEST(HandleTable, GivesEachHandleItsOwnId) {
    HandleTable<MusicId, int> table;
    const MusicId first = table.add(1);
    const MusicId second = table.add(2);

    EXPECT_NE(first, second);
    ASSERT_NE(table.find(first), nullptr);
    ASSERT_NE(table.find(second), nullptr);
    EXPECT_EQ(*table.find(first), 1);
    EXPECT_EQ(*table.find(second), 2);
}

TEST(HandleTable, FindsNothingForUnknownIds) {
    HandleTable<SoundId, int> table;
    table.add(1);

    EXPECT_EQ(table.find(SoundId{}), nullptr);
    EXPECT_EQ(table.find(SoundId{.value = 42}), nullptr);
}

TEST(HandleTable, RemoveGivesTheHandleBackOnce) {
    HandleTable<SoundId, int> table;
    const SoundId id = table.add(5);

    EXPECT_EQ(table.remove(id), 5);
    EXPECT_EQ(table.find(id), nullptr);
    EXPECT_EQ(table.remove(id), std::nullopt);
}

TEST(HandleTable, RemovedIdIsNotReused) {
    HandleTable<SoundId, int> table;
    const SoundId removed = table.add(1);
    table.remove(removed);

    EXPECT_NE(table.add(2), removed);
}

TEST(HandleTable, RemoveAllEmptiesTheTable) {
    HandleTable<MusicId, int> table;
    const MusicId first = table.add(1);
    const MusicId second = table.add(2);

    auto handles = table.remove_all();
    std::ranges::sort(handles);

    EXPECT_EQ(handles, (std::vector<int>{1, 2}));
    EXPECT_EQ(table.find(first), nullptr);
    EXPECT_EQ(table.find(second), nullptr);
    EXPECT_TRUE(table.remove_all().empty());
}
