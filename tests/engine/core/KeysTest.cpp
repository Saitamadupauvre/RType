#include <gtest/gtest.h>

#include <cstddef>
#include <set>
#include <string_view>

#include "engine/core/Keys.hpp"

using engine::core::Key;
using engine::core::key_from_name;
using engine::core::mouse_button_from_name;
using engine::core::MouseButton;

TEST(Keys, NamesFollowTheEnumerators) {
    EXPECT_EQ(key_from_name("A"), Key::A);
    EXPECT_EQ(key_from_name("Space"), Key::Space);
    EXPECT_EQ(key_from_name("Num1"), Key::Num1);
    EXPECT_EQ(key_from_name("GraveAccent"), Key::GraveAccent);
}

TEST(Keys, EveryKeyButUnknownHasAUniqueName) {
    std::set<std::string_view> names;
    for (const auto& info : engine::core::keys) {
        EXPECT_FALSE(info.name.empty());
        EXPECT_TRUE(names.insert(info.name).second) << info.name;
        EXPECT_EQ(key_from_name(info.name), info.key) << info.name;
    }
    EXPECT_EQ(names.size() + 1, engine::core::key_count);
}

TEST(Keys, RejectsUnknownAndMisspelledNames) {
    EXPECT_FALSE(key_from_name("Unknown").has_value());
    EXPECT_FALSE(key_from_name("space").has_value());
    EXPECT_FALSE(key_from_name("").has_value());
    EXPECT_FALSE(key_from_name("Spcae").has_value());
}

TEST(Keys, MouseButtonsHaveNames) {
    EXPECT_EQ(mouse_button_from_name("Left"), MouseButton::Left);
    EXPECT_EQ(mouse_button_from_name("Right"), MouseButton::Right);
    EXPECT_EQ(mouse_button_from_name("Middle"), MouseButton::Middle);
    EXPECT_FALSE(mouse_button_from_name("left").has_value());
    EXPECT_FALSE(mouse_button_from_name("Back").has_value());
    EXPECT_EQ(engine::core::mouse_button_count, 3U);
}
