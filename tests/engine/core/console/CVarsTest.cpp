#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "engine/core/console/CVars.hpp"

using engine::core::console::CVarKind;
using engine::core::console::CVarRange;
using engine::core::console::CVars;
using engine::core::console::CVarSpec;
using engine::core::console::CVarValue;
using engine::core::console::kind_name;
using engine::core::console::to_string;

namespace {

CVarSpec volume_spec() {
    return {.name = "audio.volume",
            .default_value = 0.5,
            .description = "master volume",
            .range = CVarRange{.min = 0.0, .max = 1.0}};
}

} // namespace

TEST(CVars, KeepsTheKindOfTheDefaultValue) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "debug.flag", .default_value = true}));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));
    ASSERT_TRUE(cvars.add(volume_spec()));
    ASSERT_TRUE(cvars.add({.name = "player.name", .default_value = std::string("ace")}));

    EXPECT_EQ(cvars.find("debug.flag")->kind, CVarKind::boolean);
    EXPECT_EQ(cvars.find("sv.rate")->kind, CVarKind::integer);
    EXPECT_EQ(cvars.find("audio.volume")->kind, CVarKind::number);
    EXPECT_EQ(cvars.find("player.name")->kind, CVarKind::string);
    EXPECT_EQ(cvars.get("sv.rate"), CVarValue(std::int64_t{60}));
    EXPECT_EQ(cvars.find("audio.volume")->description, "master volume");
}

TEST(CVars, ListsVariablesSortedByName) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "b", .default_value = true}));
    ASSERT_TRUE(cvars.add({.name = "a", .default_value = true}));

    const auto list = cvars.list();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].name, "a");
    EXPECT_EQ(list[1].name, "b");
}

TEST(CVars, RejectsDuplicateNames) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "debug.flag", .default_value = true}));
    const auto result = cvars.add({.name = "debug.flag", .default_value = false});
    EXPECT_FALSE(result);
    EXPECT_NE(result.error.find("already"), std::string::npos);
    EXPECT_EQ(cvars.get("debug.flag"), CVarValue(true));
}

TEST(CVars, RejectsInvalidNames) {
    CVars cvars;
    for (const std::string name : {"", "Debug", "1st", "with space", "dash-name", ".dot"}) {
        EXPECT_FALSE(cvars.add({.name = name, .default_value = true})) << name;
    }
    EXPECT_TRUE(cvars.list().empty());
}

TEST(CVars, RejectsBadRanges) {
    CVars cvars;
    EXPECT_FALSE(cvars.add({.name = "flag", .default_value = true, .range = CVarRange{0, 1}}));
    EXPECT_FALSE(
        cvars.add({.name = "label", .default_value = std::string("x"), .range = CVarRange{0, 1}}));
    EXPECT_FALSE(cvars.add({.name = "empty", .default_value = 1.0, .range = CVarRange{2, 1}}));
    EXPECT_FALSE(cvars.add({.name = "outside", .default_value = 5.0, .range = CVarRange{0, 1}}));
    EXPECT_FALSE(
        cvars.add({.name = "nan", .default_value = std::numeric_limits<double>::quiet_NaN()}));
    EXPECT_TRUE(cvars.list().empty());
}

TEST(CVars, SetChecksTheRange) {
    CVars cvars;
    ASSERT_TRUE(cvars.add(volume_spec()));

    EXPECT_TRUE(cvars.set("audio.volume", 1.0));
    const auto result = cvars.set("audio.volume", 1.5);
    EXPECT_FALSE(result);
    EXPECT_NE(result.error.find("between"), std::string::npos);
    EXPECT_EQ(cvars.get("audio.volume"), CVarValue(1.0));
}

TEST(CVars, SetRejectsAnotherKind) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "debug.flag", .default_value = true}));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));

    EXPECT_FALSE(cvars.set("debug.flag", std::int64_t{1}));
    EXPECT_FALSE(cvars.set("sv.rate", std::string("fast")));
    EXPECT_FALSE(cvars.set("sv.rate", 2.5));
    EXPECT_EQ(cvars.get("sv.rate"), CVarValue(std::int64_t{60}));
}

TEST(CVars, ConvertsBetweenIntegersAndWholeNumbers) {
    CVars cvars;
    ASSERT_TRUE(cvars.add(volume_spec()));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));

    EXPECT_TRUE(cvars.set("audio.volume", std::int64_t{1}));
    EXPECT_EQ(cvars.get("audio.volume"), CVarValue(1.0));
    EXPECT_TRUE(cvars.set("sv.rate", 30.0));
    EXPECT_EQ(cvars.get("sv.rate"), CVarValue(std::int64_t{30}));
}

TEST(CVars, UnknownNamesFail) {
    CVars cvars;
    EXPECT_FALSE(cvars.set("missing", true));
    EXPECT_FALSE(cvars.set_from_string("missing", "1"));
    EXPECT_FALSE(cvars.reset("missing"));
    EXPECT_FALSE(cvars.get("missing").has_value());
    EXPECT_FALSE(cvars.find("missing").has_value());
    EXPECT_FALSE(cvars.contains("missing"));
    EXPECT_TRUE(cvars.get_bool("missing", true));
    EXPECT_DOUBLE_EQ(cvars.get_number("missing", 4.0), 4.0);
}

TEST(CVars, ParsesTextWithTheKindOfTheVariable) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "debug.flag", .default_value = false}));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));
    ASSERT_TRUE(cvars.add(volume_spec()));
    ASSERT_TRUE(cvars.add({.name = "player.name", .default_value = std::string("ace")}));

    for (const auto* text : {"true", "ON", "yes", "1"}) {
        ASSERT_TRUE(cvars.set_from_string("debug.flag", "off"));
        EXPECT_TRUE(cvars.set_from_string("debug.flag", text));
        EXPECT_TRUE(cvars.get_bool("debug.flag", false)) << text;
    }
    EXPECT_TRUE(cvars.set_from_string("sv.rate", "30"));
    EXPECT_DOUBLE_EQ(cvars.get_number("sv.rate", 0), 30.0);
    EXPECT_TRUE(cvars.set_from_string("audio.volume", "0.25"));
    EXPECT_DOUBLE_EQ(cvars.get_number("audio.volume", 0), 0.25);
    EXPECT_TRUE(cvars.set_from_string("player.name", "red baron"));
    EXPECT_EQ(cvars.get("player.name"), CVarValue(std::string("red baron")));
}

TEST(CVars, RejectsTextThatDoesNotParse) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "debug.flag", .default_value = false}));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));
    ASSERT_TRUE(cvars.add(volume_spec()));

    EXPECT_FALSE(cvars.set_from_string("debug.flag", "maybe"));
    EXPECT_FALSE(cvars.set_from_string("sv.rate", "6O"));
    EXPECT_FALSE(cvars.set_from_string("sv.rate", "2.5"));
    EXPECT_FALSE(cvars.set_from_string("sv.rate", ""));
    EXPECT_FALSE(cvars.set_from_string("audio.volume", "loud"));
    EXPECT_FALSE(cvars.set_from_string("audio.volume", "nan"));
    EXPECT_FALSE(cvars.set_from_string("audio.volume", "0.5x"));
    EXPECT_EQ(cvars.get("sv.rate"), CVarValue(std::int64_t{60}));
    EXPECT_EQ(cvars.get("audio.volume"), CVarValue(0.5));
}

TEST(CVars, ResetRestoresTheDefault) {
    CVars cvars;
    ASSERT_TRUE(cvars.add(volume_spec()));
    ASSERT_TRUE(cvars.set("audio.volume", 0.1));
    EXPECT_TRUE(cvars.reset("audio.volume"));
    EXPECT_EQ(cvars.get("audio.volume"), CVarValue(0.5));
}

TEST(CVars, ListenersRunOnlyWhenTheValueChanges) {
    CVars cvars;
    std::vector<CVarValue> seen;
    ASSERT_TRUE(
        cvars.add(volume_spec(), [&seen](const CVarValue& value) { seen.push_back(value); }));

    EXPECT_TRUE(cvars.set("audio.volume", 0.5));
    EXPECT_TRUE(cvars.set("audio.volume", 0.75));
    EXPECT_FALSE(cvars.set("audio.volume", 2.0));
    EXPECT_TRUE(cvars.reset("audio.volume"));

    EXPECT_EQ(seen, (std::vector<CVarValue>{0.75, 0.5}));
}

TEST(CVars, TypedGettersFallBackOnAnotherKind) {
    CVars cvars;
    ASSERT_TRUE(cvars.add({.name = "player.name", .default_value = std::string("ace")}));
    ASSERT_TRUE(cvars.add({.name = "sv.rate", .default_value = std::int64_t{60}}));

    EXPECT_FALSE(cvars.get_bool("player.name", false));
    EXPECT_DOUBLE_EQ(cvars.get_number("player.name", -1.0), -1.0);
    EXPECT_DOUBLE_EQ(cvars.get_number("sv.rate", -1.0), 60.0);
}

TEST(CVars, FormatsValuesAndKinds) {
    EXPECT_EQ(to_string(true), "true");
    EXPECT_EQ(to_string(false), "false");
    EXPECT_EQ(to_string(std::int64_t{-3}), "-3");
    EXPECT_EQ(to_string(0.25), "0.25");
    EXPECT_EQ(to_string(std::string("ace")), "ace");
    EXPECT_EQ(kind_name(CVarKind::boolean), "bool");
    EXPECT_EQ(kind_name(CVarKind::integer), "int");
    EXPECT_EQ(kind_name(CVarKind::number), "number");
    EXPECT_EQ(kind_name(CVarKind::string), "string");
}
