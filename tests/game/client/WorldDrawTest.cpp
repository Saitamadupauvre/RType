#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "WorldDraw.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/graphics/IRenderer.hpp"

using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Rgba;
using engine::core::Sprite;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using rtype::client::WorldDraw;

namespace {

constexpr TextureId ship{.value = 7};

class RecordingRenderer final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return false; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] Vec2 virtual_size() const override { return {1920, 1080}; }
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return path.ends_with("missing.png") ? TextureId{} : ship;
    }
    void unload_texture(TextureId) override {}
    [[nodiscard]] Vec2 texture_size(TextureId texture) const override {
        return texture == ship ? Vec2{40, 20} : Vec2{32, 32};
    }
    void begin_frame(Color) override {}
    void end_frame() override {}
    void draw_sprite(const SpriteDraw& sprite) override { sprites.push_back(sprite); }
    void draw_rect(const Rect& rect, Color color, bool) override {
        rects.push_back(rect);
        rect_colors.push_back(color);
    }
    void draw_circle(Vec2, float, Color) override {}
    void draw_line(Vec2, Vec2, float, Color) override {}
    void draw_text(const std::string&, Vec2, float, Color) override {}
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {}; }

    std::vector<std::string> loaded;
    std::vector<SpriteDraw> sprites;
    std::vector<Rect> rects;
    std::vector<Color> rect_colors;
};

void register_components(EntityManager& world) {
    world.register_component<Position>();
    world.register_component<Sprite>();
}

void spawn(EntityManager& world, Vec2 position, Sprite sprite) {
    const auto entity = world.spawn();
    world.add<Position>(entity, position);
    world.add<Sprite>(entity, std::move(sprite));
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
    EXPECT_EQ(drawn.dest, (Rect{.x = 100, .y = 50, .width = 40, .height = 20}));
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
    EXPECT_EQ(renderer.sprites.front().dest, (Rect{.x = 5, .y = 5, .width = 32, .height = 32}));
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
