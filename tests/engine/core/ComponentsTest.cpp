#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"

using engine::core::EntityManager;
using engine::core::Hitbox;
using engine::core::Sprite;
using engine::core::Tags;
using engine::core::Vec2;

TEST(Sprite, DefaultsToNoImage) {
    const Sprite sprite;
    EXPECT_TRUE(sprite.path.empty());
}

TEST(Hitbox, DefaultsToEmptyBoxAtOrigin) {
    const Hitbox hitbox;
    EXPECT_EQ(hitbox.offset, Vec2{});
    EXPECT_EQ(hitbox.size, Vec2{});
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
    manager.register_component<Hitbox>();
    manager.register_component<Tags>();

    const auto entity = manager.spawn();
    manager.add(entity, Sprite{.path = "ship.png"});
    manager.add(entity, Hitbox{.offset = {1.0F, 2.0F}, .size = {32.0F, 16.0F}});
    manager.add(entity, Tags{"player"});

    EXPECT_EQ(manager.get<Sprite>(entity).path, "ship.png");
    EXPECT_EQ(manager.get<Hitbox>(entity).size, (Vec2{32.0F, 16.0F}));
    EXPECT_TRUE(manager.get<Tags>(entity).has("player"));
}
