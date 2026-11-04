#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/network/StringTable.hpp"

#include "protocol/WireValue.hpp"

namespace {

using engine::core::AssetName;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::network::AssetId;
using engine::network::from_wire;
using engine::network::no_asset;
using engine::network::StringCategory;
using engine::network::StringTable;
using engine::network::to_wire;
using engine::network::WireValue;

constexpr std::uint16_t ship_id = 5;

StringTable texture_strings() {
    StringTable table;
    EXPECT_TRUE(table.insert(
        {.category = StringCategory::texture, .id = ship_id, .name = "sprites/ship.png"}));
    EXPECT_TRUE(table.insert({.category = StringCategory::sound, .id = 6, .name = "shot.wav"}));
    return table;
}

std::vector<FieldValue> scalar_samples() {
    return {FieldValue{1.5F},
            FieldValue{std::int64_t{-7}},
            FieldValue{true},
            FieldValue{std::string("hi")},
            FieldValue{Vec2{1.F, 2.F}},
            FieldValue{Rgba{}},
            FieldValue{AssetName{"sprites/ship.png"}}};
}

} // namespace

TEST(WireValue, ScalarsRoundTripThroughTheWire) {
    const StringTable strings = texture_strings();

    for (const auto& value : scalar_samples()) {
        const auto wire = to_wire(value, strings);
        ASSERT_TRUE(wire.has_value());
        EXPECT_EQ(from_wire(*wire, strings), value);
    }
}

TEST(WireValue, KindMatchesTheKindOfTheFieldValue) {
    const StringTable strings = texture_strings();

    for (const auto& value : scalar_samples()) {
        const auto wire = to_wire(value, strings);
        ASSERT_TRUE(wire.has_value());
        EXPECT_EQ(engine::network::kind_of(*wire), engine::core::kind_of(value));
    }
}

TEST(WireValue, SendsADeclaredTextureAsItsId) {
    EXPECT_EQ(to_wire(AssetName{"sprites/ship.png"}, texture_strings()),
              WireValue{AssetId{ship_id}});
}

TEST(WireValue, SendsAnEmptyAssetAsNoAsset) {
    EXPECT_EQ(to_wire(AssetName{}, texture_strings()), WireValue{AssetId{no_asset}});
    EXPECT_EQ(from_wire(AssetId{no_asset}, texture_strings()), FieldValue{AssetName{}});
}

TEST(WireValue, RefusesATextureMissingFromTheTable) {
    EXPECT_FALSE(to_wire(AssetName{"sprites/unknown.png"}, texture_strings()));
    EXPECT_FALSE(to_wire(AssetName{"shot.wav"}, texture_strings()));
}

TEST(WireValue, RefusesAnUnknownTextureId) {
    EXPECT_FALSE(from_wire(AssetId{6}, texture_strings()));
    EXPECT_FALSE(from_wire(AssetId{99}, texture_strings()));
}

TEST(WireValue, RefusesAList) { EXPECT_FALSE(to_wire(FieldList{}, texture_strings())); }

namespace {

constexpr std::uint16_t space_id = 2;

StringTable shader_strings() {
    StringTable table = texture_strings();
    EXPECT_TRUE(table.insert(
        {.category = StringCategory::shader, .id = space_id, .name = "shaders/space"}));
    return table;
}

} // namespace

TEST(WireValue, MapsEachAssetTypeToItsCategory) {
    EXPECT_EQ(engine::network::asset_category(engine::core::AssetType::Texture),
              StringCategory::texture);
    EXPECT_EQ(engine::network::asset_category(engine::core::AssetType::Shader),
              StringCategory::shader);
}

TEST(WireValue, SendsAShaderAsItsShaderId) {
    EXPECT_EQ(
        to_wire(AssetName{"shaders/space"}, shader_strings(), engine::core::AssetType::Shader),
        WireValue{AssetId{space_id}});
}

TEST(WireValue, ReadsAShaderIdFromTheShaderCategory) {
    EXPECT_EQ(from_wire(AssetId{space_id}, shader_strings(), engine::core::AssetType::Shader),
              FieldValue{AssetName{"shaders/space"}});
}

TEST(WireValue, NeverMixesTextureAndShaderIds) {
    const StringTable strings = shader_strings();

    EXPECT_EQ(to_wire(AssetName{"shaders/space"}, strings), std::nullopt);
    EXPECT_EQ(to_wire(AssetName{"sprites/ship.png"}, strings, engine::core::AssetType::Shader),
              std::nullopt);
    EXPECT_EQ(from_wire(AssetId{ship_id}, strings, engine::core::AssetType::Shader), std::nullopt);
}
