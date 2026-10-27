#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "SnapshotView.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/StringTable.hpp"

using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using engine::network::EntitySnapshot;
using engine::network::StringCategory;
using engine::network::StringTable;
using rtype::client::SnapshotView;

namespace {

constexpr std::uint16_t ship_type = 1;
constexpr std::uint16_t unknown_type = 9;
constexpr TextureId ship_texture{.value = 3};

struct DrawnCircle {
    Vec2 center;
    float radius;
};

class RecordingRenderer final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return false; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] Vec2 virtual_size() const override { return {1920, 1080}; }
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return ship_texture;
    }
    void unload_texture(TextureId) override {}
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {96, 36}; }
    void begin_frame(Color) override {}
    void end_frame() override {}
    void draw_sprite(const SpriteDraw& sprite) override { sprites.push_back(sprite); }
    void draw_rect(const Rect& rect, Color color, bool) override {
        rects.push_back(rect);
        rect_colors.push_back(color);
    }
    void draw_circle(Vec2 center, float radius, Color) override {
        circles.push_back({.center = center, .radius = radius});
    }
    void draw_line(Vec2, Vec2, float, Color) override {}
    void draw_text(const std::string&, Vec2, float, Color) override {}
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {}; }

    std::vector<std::string> loaded;
    std::vector<SpriteDraw> sprites;
    std::vector<Rect> rects;
    std::vector<Color> rect_colors;
    std::vector<DrawnCircle> circles;
};

StringTable ship_strings() {
    StringTable strings;
    strings.insert({.category = StringCategory::type, .id = ship_type, .name = "ship"});
    return strings;
}

const StringTable strings = ship_strings();

EntitySnapshot entity(std::uint16_t type_id, float x, std::uint8_t owner) {
    return {.entity_id = 1,
            .type_id = type_id,
            .x = x,
            .y = 20.F,
            .vx = 0.F,
            .vy = 0.F,
            .owner = owner,
            .color = {.r = 10, .g = 20, .b = 30, .a = 255}};
}

} // namespace

TEST(SnapshotView, SpriteIsNamedAfterTheEntityType) {
    EXPECT_EQ(SnapshotView::sprite_of("bydo"), "sprites/bydo.png");
}

TEST(SnapshotView, DrawsEachEntityWithTheSpriteOfItsTypeAndItsColor) {
    RecordingRenderer renderer;
    SnapshotView view(renderer, "assets");

    view.draw({entity(ship_type, 5.F, 2)}, strings, 1);

    ASSERT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.loaded.front(),
              (std::filesystem::path("assets") / "sprites/ship.png").string());
    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x, 5.F);
    EXPECT_EQ(renderer.sprites.front().tint, (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST(SnapshotView, EntityOfUnknownTypeIsAColoredSquare) {
    RecordingRenderer renderer;
    SnapshotView view(renderer, "assets");

    view.draw({entity(unknown_type, 5.F, 0)}, strings, 1);

    EXPECT_TRUE(renderer.sprites.empty());
    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_EQ(renderer.rect_colors.front(), (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST(SnapshotView, MarksOnlyTheLocalPlayerEntities) {
    RecordingRenderer renderer;
    SnapshotView view(renderer, "assets");

    view.draw({entity(ship_type, 5.F, 1), entity(ship_type, 200.F, 2)}, strings, 1);

    ASSERT_EQ(renderer.circles.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.circles.front().center.x, 5.F);
    EXPECT_LT(renderer.circles.front().center.y, 20.F);
}

TEST(SnapshotView, UnownedEntitiesAreNeverMarkedBeforeAccept) {
    RecordingRenderer renderer;
    SnapshotView view(renderer, "assets");

    view.draw({entity(ship_type, 5.F, 0)}, strings, 0);

    EXPECT_TRUE(renderer.circles.empty());
}

TEST(SnapshotView, DrawsOnlyTheNewestSnapshot) {
    RecordingRenderer renderer;
    SnapshotView view(renderer, "assets");
    view.draw({entity(ship_type, 5.F, 0), entity(ship_type, 50.F, 0)}, strings, 0);
    renderer.sprites.clear();

    view.draw({entity(ship_type, 90.F, 0)}, strings, 0);

    ASSERT_EQ(renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.sprites.front().dest.x, 90.F);
}
