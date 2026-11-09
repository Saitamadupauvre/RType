#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "clay/ClayContext.hpp"

using editor::ClayContext;

namespace {

constexpr Clay_Dimensions window{.width = 800, .height = 600};

Clay_Dimensions fixed_measure(std::string_view text, const Clay_TextElementConfig& config) {
    return {.width = static_cast<float>(text.size()) * 10.F,
            .height = static_cast<float>(config.fontSize)};
}

} // namespace

TEST(ClayContext, TextIsMeasuredWithTheGivenFunction) {
    std::string measured;
    const ClayContext clay(window, [&measured](std::string_view text, const auto& config) {
        measured = text;
        return fixed_measure(text, config);
    });

    Clay_BeginLayout();
    CLAY({.id = CLAY_ID("Label")}) {
        CLAY_TEXT(CLAY_STRING("hello"), CLAY_TEXT_CONFIG({.fontSize = 20}));
    }
    Clay_EndLayout();

    EXPECT_EQ(measured, "hello");
    const Clay_ElementData label = Clay_GetElementData(CLAY_ID("Label"));
    ASSERT_TRUE(label.found);
    EXPECT_FLOAT_EQ(label.boundingBox.width, 50.F);
    EXPECT_FLOAT_EQ(label.boundingBox.height, 20.F);
}

TEST(ClayContext, CleanLayoutReportsNoError) {
    const ClayContext clay(window, fixed_measure);

    Clay_BeginLayout();
    CLAY({.id = CLAY_ID("Only")}) {}
    Clay_EndLayout();

    EXPECT_TRUE(clay.errors().empty());
}

TEST(ClayContext, DuplicateIdIsReported) {
    const ClayContext clay(window, fixed_measure);

    Clay_BeginLayout();
    CLAY({.id = CLAY_ID("Twice")}) {}
    CLAY({.id = CLAY_ID("Twice")}) {}
    Clay_EndLayout();

    EXPECT_FALSE(clay.errors().empty());
}
