#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "../graphics/fixtures/NullRenderer.hpp"
#include "WorldDraw.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Transform.hpp"
#include "engine/graphics/IRenderer.hpp"

using engine::app::WorldDraw;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Rotation;
using engine::core::Scale;
using engine::core::Sprite;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;

namespace {

constexpr TextureId ship{.value = 7};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return path.ends_with("missing.png") ? TextureId{} : ship;
    }
    [[nodiscard]] Vec2 texture_size(TextureId texture) const override {
        return texture == ship ? Vec2{40, 20} : Vec2{32, 32};
    }
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

void register_components(EntityManager& world) {
    world.register_component<Position>();
    world.register_component<Sprite>();
    world.register_component<Rotation>();
    world.register_component<Scale>();
}

Entity spawn(EntityManager& world, Vec2 position, Sprite sprite) {
    const auto entity = world.spawn();
    world.add<Position>(entity, position);
    world.add<Sprite>(entity, std::move(sprite));
    return entity;
}

} // namespace

TEST(WorldDraw, DrawsSpriteAtEntityPositionWithTextureSize) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {100, 50},
          {.path = "ship.png", .color = Rgba{.r = 10, .g = 20, .b = 30, .a = 40}});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.texture, ship);
    EXPECT_EQ(drawn.source, (Rect{.x = 0, .y = 0, .width = 40, .height = 20}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 120, .y = 60, .width = 40, .height = 20}));
    EXPECT_EQ(drawn.origin, (Vec2{20, 10}));
    EXPECT_EQ(drawn.rotation, 0.F);
    EXPECT_EQ(drawn.tint, (Color{.r = 10, .g = 20, .b = 30, .a = 40}));
}

TEST(WorldDraw, LoadsTexturesFromTheAssetsFolder) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {}, {.path = "ship.png", .color = {}});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.loaded.front(), (std::filesystem::path("assets") / "ship.png").string());
}

TEST(WorldDraw, LoadsEachTextureOnce) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {}, {.path = "ship.png", .color = {}});
    spawn(world, {10, 10}, {.path = "ship.png", .color = {}});
    WorldDraw draw(renderer, "assets");

    draw.draw(world);
    draw.draw(world);

    EXPECT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.sprites.size(), 4U);
}

TEST(WorldDraw, MissingTextureIsDrawnAsPlaceholderAndNotReloaded) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {5, 5}, {.path = "missing.png", .color = {}});
    WorldDraw draw(renderer, "assets");

    draw.draw(world);
    draw.draw(world);

    EXPECT_EQ(renderer.loaded.size(), 1U);
    ASSERT_EQ(renderer.sprites.size(), 2U);
    EXPECT_EQ(renderer.sprites.front().texture, TextureId{});
    EXPECT_EQ(renderer.sprites.front().dest, (Rect{.x = 21, .y = 21, .width = 32, .height = 32}));
}

TEST(WorldDraw, SpriteWithoutPathIsAColoredSquare) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {3, 4}, {.path = "", .color = Rgba{.r = 255, .g = 0, .b = 0, .a = 255}});

    WorldDraw(renderer, "assets").draw(world);

    EXPECT_TRUE(renderer.loaded.empty());
    EXPECT_TRUE(renderer.sprites.empty());
    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_EQ(renderer.rects.front(), (Rect{.x = 3, .y = 4, .width = 64, .height = 64}));
    EXPECT_EQ(renderer.rect_colors.front(), (Color{.r = 255, .g = 0, .b = 0, .a = 255}));
}

TEST(WorldDraw, EntityWithoutSpriteIsNotDrawn) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    world.add<Position>(world.spawn(), Vec2{1, 1});

    WorldDraw(renderer, "assets").draw(world);

    EXPECT_TRUE(renderer.sprites.empty());
    EXPECT_TRUE(renderer.rects.empty());
}

TEST(WorldDraw, PreloadedTextureIsNotLoadedAgainWhenDrawn) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    spawn(world, {}, {.path = "ship.png", .color = {}});
    WorldDraw draw(renderer, "assets");

    EXPECT_TRUE(draw.preload("ship.png"));
    draw.draw(world);

    EXPECT_EQ(renderer.loaded.size(), 1U);
    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_EQ(renderer.sprites.front().texture, ship);
}

TEST(WorldDraw, PreloadReportsAMissingOrRejectedTexture) {
    RecordingRenderer renderer;
    WorldDraw draw(renderer, "assets");

    EXPECT_FALSE(draw.preload("missing.png"));
    EXPECT_FALSE(draw.preload("../secret.png"));
    EXPECT_FALSE(draw.preload("missing.png"));

    EXPECT_EQ(renderer.loaded.size(), 1U);
}

TEST(WorldDraw, WorksWhenRotationAndScaleAreNotRegistered) {
    RecordingRenderer renderer;
    EntityManager world;
    world.register_component<Position>();
    world.register_component<Sprite>();
    spawn(world, {100, 50}, {.path = "ship.png", .color = {}});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_EQ(renderer.sprites.front().dest, (Rect{.x = 120, .y = 60, .width = 40, .height = 20}));
}

TEST(WorldDraw, ScaleMultipliesTheDrawnSizeFromTheTopLeftCorner) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    const Entity entity = spawn(world, {100, 50}, {.path = "ship.png", .color = {}});
    world.add(entity, Scale{.factor = {2, 3}});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.source, (Rect{.x = 0, .y = 0, .width = 40, .height = 20}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 140, .y = 80, .width = 80, .height = 60}));
    EXPECT_EQ(drawn.origin, (Vec2{40, 30}));
}

TEST(WorldDraw, NegativeScaleMirrorsTheSourceAndKeepsAPositiveSize) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    const Entity entity = spawn(world, {0, 0}, {.path = "ship.png", .color = {}});
    world.add(entity, Scale{.factor = {-1, -2}});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.source, (Rect{.x = 0, .y = 0, .width = -40, .height = -20}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 20, .y = 20, .width = 40, .height = 40}));
}

TEST(WorldDraw, RotationIsPassedThroughAroundTheSpriteCenter) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    const Entity entity = spawn(world, {10, 10}, {.path = "ship.png", .color = {}});
    world.add(entity, Rotation{.degrees = 90});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.rotation, 90.F);
    EXPECT_EQ(drawn.origin, (Vec2{20, 10}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 30, .y = 20, .width = 40, .height = 20}));
}

TEST(WorldDraw, ColoredSquareIsScaledButNotRotated) {
    RecordingRenderer renderer;
    EntityManager world;
    register_components(world);
    const Entity entity = spawn(world, {3, 4}, {.path = "", .color = {}});
    world.add(entity, Scale{.factor = {-0.5F, 2}});
    world.add(entity, Rotation{.degrees = 45});

    WorldDraw(renderer, "assets").draw(world);

    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_EQ(renderer.rects.front(), (Rect{.x = 3, .y = 4, .width = 32, .height = 128}));
}
