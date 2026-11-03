#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../graphics/fixtures/NullRenderer.hpp"
#include "Fields.hpp"
#include "ShaderLibrary.hpp"
#include "SnapshotView.hpp"
#include "TextureLibrary.hpp"
#include "WorldDraw.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"

using engine::app::SnapshotView;
using engine::core::AssetName;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using engine::network::EntitySnapshot;
using engine::network::ReplicatedValue;

namespace {

constexpr std::uint16_t ship_type = 1;
constexpr TextureId ship_texture{.value = 3};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return ship_texture;
    }
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {96, 36}; }
    void draw_sprite(const SpriteDraw& sprite) override { sprites.push_back(sprite); }
    void draw_rect(const Rect& rect, Color color, bool) override {
        rects.push_back(rect);
        rect_colors.push_back(color);
    }

    std::vector<std::string> loaded;
    std::vector<SpriteDraw> sprites;
    std::vector<Rect> rects;
    std::vector<Color> rect_colors;
};

const FieldRegistry fields = engine::app::replicated_fields();

ReplicatedValue value_of(std::string_view component, std::string_view field, FieldValue value) {
    return {.field = fields.find_field(component, field), .value = std::move(value)};
}

EntitySnapshot entity(std::string sprite, float x, std::int64_t owner) {
    return {.entity_id = 1,
            .type_id = ship_type,
            .fields = {value_of("Position", "pos", Vec2{x, 20.F}),
                       value_of("PhysicsBody", "velocity", Vec2{3.F, 0.F}),
                       value_of("Replicated", "owner", owner),
                       value_of("Sprite", "path", AssetName{std::move(sprite)}),
                       value_of("Sprite", "color", Rgba{.r = 10, .g = 20, .b = 30, .a = 255})}};
}

class Screen {
public:
    Screen()
        : _textures(renderer, "assets"), _shaders(renderer, "assets"),
          _draw(renderer, _textures, _shaders), _view(fields) {}

    void show(const std::vector<EntitySnapshot>& entities) {
        _draw.draw(_view.mirror(entities), 0.F);
    }

    RecordingRenderer renderer;

private:
    engine::app::TextureLibrary _textures;
    engine::app::ShaderLibrary _shaders;
    engine::app::WorldDraw _draw;
    SnapshotView _view;
};

} // namespace

TEST(SnapshotView, DrawsEachEntityWithItsReplicatedSpriteAndColor) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;

    screen.show({entity("sprites/boss/core.png", 5.F, 2)});

    ASSERT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.loaded.front(),
              (std::filesystem::path("assets") / "sprites/boss/core.png").string());
    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x - renderer.sprites.front().origin.x, 5.F);
    EXPECT_EQ(renderer.sprites.front().tint, (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST(SnapshotView, EntityWithoutSpriteIsAColoredSquare) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;

    screen.show({entity("", 5.F, 0)});

    EXPECT_TRUE(renderer.loaded.empty());
    EXPECT_TRUE(renderer.sprites.empty());
    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_EQ(renderer.rect_colors.front(), (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST(SnapshotView, LoadsASpriteSharedByManyEntitiesOnce) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;

    screen.show({entity("sprites/ship.png", 5.F, 0), entity("sprites/ship.png", 50.F, 0)});
    screen.show({entity("sprites/ship.png", 90.F, 0)});

    EXPECT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.sprites.size(), 3U);
}

TEST(SnapshotView, DrawsOnlyTheNewestSnapshot) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;
    screen.show({entity("sprites/ship.png", 5.F, 0), entity("sprites/ship.png", 50.F, 0)});
    renderer.sprites.clear();

    screen.show({entity("sprites/ship.png", 90.F, 0)});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x - renderer.sprites.front().origin.x, 90.F);
}

TEST(SnapshotView, EntityWithoutColorIsDrawnWhite) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;
    const EntitySnapshot plain{
        .entity_id = 1,
        .type_id = ship_type,
        .fields = {value_of("Position", "pos", Vec2{5.F, 20.F}),
                   value_of("Sprite", "path", AssetName{"sprites/ship.png"})}};

    screen.show({plain});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_EQ(renderer.sprites.front().tint, (Color{.r = 255, .g = 255, .b = 255, .a = 255}));
}

TEST(SnapshotView, IgnoresFieldsOfComponentsItDoesNotDraw) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;

    screen.show({entity("sprites/ship.png", 5.F, 1'000)});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x - renderer.sprites.front().origin.x, 5.F);
}

TEST(SnapshotView, CameraEntitiesAreNotDrawn) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;
    EntitySnapshot camera{
        .entity_id = 2,
        .type_id = ship_type,
        .fields = {value_of("Position", "pos", Vec2{5.F, 20.F}), value_of("Camera", "zoom", 1.F)}};

    screen.show({camera, entity("sprites/ship.png", 40.F, 0)});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x - renderer.sprites.front().origin.x, 40.F);
    EXPECT_TRUE(renderer.rects.empty());
}

TEST(SnapshotView, EntityWithoutRotationOrScaleIsDrawnUpright) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;

    screen.show({entity("sprites/ship.png", 5.F, 0)});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_EQ(renderer.sprites.front().rotation, 0.F);
    EXPECT_EQ(renderer.sprites.front().dest,
              (Rect{.x = 53.F, .y = 38.F, .width = 96.F, .height = 36.F}));
}

TEST(SnapshotView, ReplicatedRotationAndScaleAreDrawn) {
    Screen screen;
    RecordingRenderer& renderer = screen.renderer;
    EntitySnapshot turned = entity("sprites/ship.png", 5.F, 0);
    turned.fields.push_back(value_of("Rotation", "degrees", 30.F));
    turned.fields.push_back(value_of("Scale", "x", -2.F));
    turned.fields.push_back(value_of("Scale", "y", 0.5F));

    screen.show({turned});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.rotation, 30.F);
    EXPECT_EQ(drawn.source, (Rect{.x = 0.F, .y = 0.F, .width = -96.F, .height = 36.F}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 101.F, .y = 29.F, .width = 192.F, .height = 18.F}));
}
