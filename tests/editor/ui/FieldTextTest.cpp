#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

#include "ui/FieldText.hpp"

using editor::color_from_hex;
using editor::color_to_hex;
using engine::core::FieldValue;
using engine::core::Rgba;

TEST(FieldText, ColorIsWrittenAsUppercaseRgba) {
    EXPECT_EQ(color_to_hex({.r = 10, .g = 20, .b = 255, .a = 128}), "#0A14FF80");
}

TEST(FieldText, ColorReadsBackWhatItWrites) {
    const Rgba color{.r = 1, .g = 2, .b = 3, .a = 4};
    EXPECT_EQ(color_from_hex(color_to_hex(color)), color);
}

TEST(FieldText, ColorWithoutAlphaIsOpaque) {
    EXPECT_EQ(color_from_hex("#ff8000"), (Rgba{.r = 255, .g = 128, .b = 0, .a = 255}));
}

TEST(FieldText, ColorHashIsOptional) {
    EXPECT_EQ(color_from_hex("FF800010"), (Rgba{.r = 255, .g = 128, .b = 0, .a = 16}));
}

TEST(FieldText, ColorRejectsWrongLengths) {
    EXPECT_EQ(color_from_hex(""), std::nullopt);
    EXPECT_EQ(color_from_hex("#FFF"), std::nullopt);
    EXPECT_EQ(color_from_hex("#FF8000F"), std::nullopt);
    EXPECT_EQ(color_from_hex("#FF8000FF00"), std::nullopt);
}

TEST(FieldText, ColorRejectsNonHexDigits) {
    EXPECT_EQ(color_from_hex("#GG8000"), std::nullopt);
    EXPECT_EQ(color_from_hex("#FF 000"), std::nullopt);
    EXPECT_EQ(color_from_hex("#+F8000"), std::nullopt);
}

TEST(FieldText, DescribesEveryKind) {
    EXPECT_EQ(editor::describe(FieldValue{1.5F}), "1.5");
    EXPECT_EQ(editor::describe(FieldValue{std::int64_t{-3}}), "-3");
    EXPECT_EQ(editor::describe(FieldValue{true}), "true");
    EXPECT_EQ(editor::describe(FieldValue{std::string("a b")}), "a b");
    EXPECT_EQ(editor::describe(FieldValue{engine::core::Vec2{1.F, 2.5F}}), "1, 2.5");
    EXPECT_EQ(editor::describe(FieldValue{Rgba{}}), "#FFFFFFFF");
    EXPECT_EQ(editor::describe(FieldValue{engine::core::AssetName{.path = "ship.png"}}),
              "ship.png");
}

TEST(FieldText, DescribesAListByItsSize) {
    engine::core::FieldList list{.element = engine::core::FieldKind::Int, .items = {}};
    EXPECT_EQ(editor::describe(FieldValue{list}), "0 items");
    list.items.emplace_back(std::int64_t{1});
    EXPECT_EQ(editor::describe(FieldValue{list}), "1 item");
}
