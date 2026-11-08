#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "text/ScriptLocation.hpp"

using editor::script_location;
using editor::ScriptLocation;

TEST(ScriptLocation, FindsTheFileAndLineOfALuaError) {
    EXPECT_EQ(script_location("[ERROR] lua: update: player.lua:12: attempt to call a nil value"),
              (ScriptLocation{.file = "player.lua", .line = 12}));
}

TEST(ScriptLocation, KeepsSubfolders) {
    EXPECT_EQ(script_location("enemies/bydo.lua:3: boom"),
              (ScriptLocation{.file = "enemies/bydo.lua", .line = 3}));
}

TEST(ScriptLocation, TakesTheFirstValidPlace) {
    EXPECT_EQ(script_location("x.lua: no line, then main.lua:7: here"),
              (ScriptLocation{.file = "main.lua", .line = 7}));
}

TEST(ScriptLocation, IgnoresTextWithoutAPlace) {
    EXPECT_EQ(script_location(""), std::nullopt);
    EXPECT_EQ(script_location("scene saved"), std::nullopt);
    EXPECT_EQ(script_location("player.lua is missing"), std::nullopt);
    EXPECT_EQ(script_location(".lua:4: nameless"), std::nullopt);
    EXPECT_EQ(script_location("player.lua:abc: not a line"), std::nullopt);
    EXPECT_EQ(script_location("player.lua:12 no colon"), std::nullopt);
}

TEST(ScriptLocation, RefusesPathsOutsideTheScripts) {
    EXPECT_EQ(script_location("/etc/evil.lua:1: x"), std::nullopt);
    EXPECT_EQ(script_location("../secret.lua:1: x"), std::nullopt);
    EXPECT_EQ(script_location("lib/../../x.lua:1: x"), std::nullopt);
}

TEST(ScriptLocation, RefusesLinesOutOfRange) {
    EXPECT_EQ(script_location("a.lua:0: x"), std::nullopt);
    EXPECT_EQ(script_location("a.lua:99999999999999999999: x"), std::nullopt);
}
