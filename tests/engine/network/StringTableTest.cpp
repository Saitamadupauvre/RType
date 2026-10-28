#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/network/StringTable.hpp"

using engine::network::max_string_name_length;
using engine::network::reserved_string_id;
using engine::network::StringCategory;
using engine::network::StringEntry;
using engine::network::StringTable;

TEST(StringTable, AddGivesIdsFromOneInOrder) {
    StringTable table;

    EXPECT_EQ(table.add(StringCategory::type, "player"), 1U);
    EXPECT_EQ(table.add(StringCategory::type, "bydo"), 2U);
    EXPECT_EQ(table.id_of(StringCategory::type, "bydo"), 2U);
    ASSERT_NE(table.name_of(StringCategory::type, 1), nullptr);
    EXPECT_EQ(*table.name_of(StringCategory::type, 1), "player");
}

TEST(StringTable, CategoriesHaveTheirOwnIds) {
    StringTable table;

    EXPECT_EQ(table.add(StringCategory::type, "bydo"), 1U);
    EXPECT_EQ(table.add(StringCategory::sound, "bydo"), 1U);
    EXPECT_EQ(table.add(StringCategory::sound, "laser.wav"), 2U);
    EXPECT_EQ(table.id_of(StringCategory::music, "bydo"), std::nullopt);
    EXPECT_EQ(table.entries().size(), 3U);
}

TEST(StringTable, AddingTheSameNameTwiceKeepsItsId) {
    StringTable table;
    table.add(StringCategory::music, "stage1.ogg");
    table.add(StringCategory::music, "boss.ogg");

    EXPECT_EQ(table.add(StringCategory::music, "stage1.ogg"), 1U);
    EXPECT_EQ(table.entries().size(), 2U);
}

TEST(StringTable, AddRejectsEmptyAndOversizedNames) {
    StringTable table;

    EXPECT_EQ(table.add(StringCategory::type, ""), std::nullopt);
    EXPECT_EQ(table.add(StringCategory::type, std::string(max_string_name_length + 1, 'a')),
              std::nullopt);
    EXPECT_EQ(table.add(StringCategory::type, std::string(max_string_name_length, 'a')), 1U);
    EXPECT_EQ(table.entries().size(), 1U);
}

TEST(StringTable, AddContinuesAfterTheHighestInsertedId) {
    StringTable table;
    ASSERT_TRUE(table.insert({.category = StringCategory::sound, .id = 7, .name = "laser.wav"}));

    EXPECT_EQ(table.add(StringCategory::sound, "explosion.wav"), 8U);
}

TEST(StringTable, AddFailsOnceTheCategoryHasNoIdLeft) {
    StringTable table;
    ASSERT_TRUE(table.insert(
        {.category = StringCategory::music, .id = reserved_string_id - 1, .name = "last.ogg"}));

    EXPECT_EQ(table.add(StringCategory::music, "more.ogg"), std::nullopt);
    EXPECT_EQ(table.add(StringCategory::sound, "more.wav"), 1U);
}

TEST(StringTable, InsertRejectsDuplicatesWithinACategory) {
    StringTable table;
    ASSERT_TRUE(table.insert({.category = StringCategory::type, .id = 3, .name = "bydo"}));

    EXPECT_FALSE(table.insert({.category = StringCategory::type, .id = 3, .name = "boss"}));
    EXPECT_FALSE(table.insert({.category = StringCategory::type, .id = 4, .name = "bydo"}));
    EXPECT_TRUE(table.insert({.category = StringCategory::field, .id = 3, .name = "bydo"}));
    EXPECT_EQ(table.entries().size(), 2U);
}

TEST(StringTable, InsertRejectsReservedIdAndInvalidNames) {
    StringTable table;

    EXPECT_FALSE(
        table.insert({.category = StringCategory::music, .id = reserved_string_id, .name = "a"}));
    EXPECT_FALSE(table.insert({.category = StringCategory::type, .id = 1, .name = ""}));
    EXPECT_FALSE(table.insert({.category = StringCategory::type,
                               .id = 1,
                               .name = std::string(max_string_name_length + 1, 'a')}));
    EXPECT_TRUE(table.entries().empty());
}

TEST(StringTable, UnknownLookupsFindNothing) {
    StringTable table;
    table.add(StringCategory::type, "bydo");

    EXPECT_EQ(table.name_of(StringCategory::type, 2), nullptr);
    EXPECT_EQ(table.name_of(StringCategory::sound, 1), nullptr);
    EXPECT_EQ(table.id_of(StringCategory::type, "boss"), std::nullopt);
}

TEST(StringTable, EntriesOfACategoryKeepInsertionOrder) {
    StringTable table;
    table.add(StringCategory::sound, "b.wav");
    table.add(StringCategory::type, "bydo");
    table.add(StringCategory::sound, "a.wav");

    EXPECT_EQ(
        table.entries(StringCategory::sound),
        (std::vector<StringEntry>{{.category = StringCategory::sound, .id = 1, .name = "b.wav"},
                                  {.category = StringCategory::sound, .id = 2, .name = "a.wav"}}));
}
