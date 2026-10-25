#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"

using engine::core::EntityManager;
using engine::core::Sprite;
using engine::core::Tags;

TEST(Sprite, DefaultsToNoImageAndWhite) {
    const Sprite sprite;
    EXPECT_TRUE(sprite.path.empty());
    EXPECT_EQ(sprite.color, (engine::core::Rgba{.r = 255, .g = 255, .b = 255, .a = 255}));
}

TEST(Tags, StartsEmpty) {
    const Tags tags;
    EXPECT_TRUE(tags.labels().empty());
    EXPECT_FALSE(tags.has("enemy"));
}

TEST(Tags, HasAddedLabel) {
    Tags tags;
    EXPECT_TRUE(tags.add("enemy"));
    EXPECT_TRUE(tags.has("enemy"));
    EXPECT_FALSE(tags.has("player"));
}

TEST(Tags, IgnoresDuplicateLabel) {
    Tags tags;
    tags.add("enemy");
    EXPECT_FALSE(tags.add("enemy"));
    EXPECT_EQ(tags.labels().size(), 1U);
}

TEST(Tags, InitializerListDropsDuplicates) {
    const Tags tags{"enemy", "boss", "enemy"};
    EXPECT_EQ(tags.labels(), (std::vector<std::string>{"enemy", "boss"}));
}

TEST(Tags, RemovesLabel) {
    Tags tags{"enemy", "boss"};
    EXPECT_TRUE(tags.remove("enemy"));
    EXPECT_FALSE(tags.has("enemy"));
    EXPECT_EQ(tags.labels(), (std::vector<std::string>{"boss"}));
}

TEST(Tags, RemoveMissingLabelReturnsFalse) {
    Tags tags{"enemy"};
    EXPECT_FALSE(tags.remove("player"));
    EXPECT_EQ(tags.labels().size(), 1U);
}

TEST(Tags, EmptyLabelIsALabel) {
    Tags tags;
    EXPECT_TRUE(tags.add(""));
    EXPECT_TRUE(tags.has(""));
}

TEST(Components, StoredInEntityManager) {
    EntityManager manager;
    manager.register_component<Sprite>();
    manager.register_component<Tags>();

    const auto entity = manager.spawn();
    manager.add(entity, Sprite{.path = "ship.png", .color = {}});
    manager.add(entity, Tags{"player"});

    EXPECT_EQ(manager.get<Sprite>(entity).path, "ship.png");
    EXPECT_TRUE(manager.get<Tags>(entity).has("player"));
}
