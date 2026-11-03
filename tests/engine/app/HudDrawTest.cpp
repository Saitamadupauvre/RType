#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../graphics/fixtures/NullRenderer.hpp"
#include "HudDraw.hpp"
#include "TextureLibrary.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/script/HudCommand.hpp"

using engine::app::HudDraw;
using engine::app::TextureLibrary;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using engine::script::HudRect;
using engine::script::HudSprite;
using engine::script::HudText;

namespace {

constexpr TextureId heart{.value = 9};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        return heart;
    }
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {16, 8}; }
    void draw_sprite(const SpriteDraw& sprite) override {
        calls.emplace_back("sprite");
        sprites.push_back(sprite);
    }
    void draw_rect(const Rect& rect, Color color, bool filled) override {
        calls.emplace_back(filled ? "rect" : "outline");
        rects.push_back(rect);
        rect_colors.push_back(color);
    }
    void draw_text(const std::string& text, Vec2 position, float size, Color color) override {
        calls.push_back("text " + text);
        text_positions.push_back(position);
        text_sizes.push_back(size);
        text_colors.push_back(color);
    }

    std::vector<std::string> calls;
    std::vector<std::string> loaded;
    std::vector<SpriteDraw> sprites;
    std::vector<Rect> rects;
    std::vector<Color> rect_colors;
    std::vector<Vec2> text_positions;
    std::vector<float> text_sizes;
    std::vector<Color> text_colors;
};

} // namespace

TEST(HudDraw, ReplaysCommandsInOrder) {
    RecordingRenderer renderer;
    TextureLibrary textures(renderer, "assets");

    HudDraw(renderer, textures)
        .draw({HudRect{.position = {1, 2}, .size = {3, 4}, .color = {}, .filled = false},
               HudText{.text = "HP", .position = {5, 6}, .size = 20, .color = {}},
               HudRect{.position = {0, 0}, .size = {1, 1}, .color = {}, .filled = true}});

    EXPECT_EQ(renderer.calls, (std::vector<std::string>{"outline", "text HP", "rect"}));
    EXPECT_EQ(renderer.rects.front(), (Rect{.x = 1, .y = 2, .width = 3, .height = 4}));
}

TEST(HudDraw, TextKeepsItsPositionSizeAndColor) {
    RecordingRenderer renderer;
    TextureLibrary textures(renderer, "assets");

    HudDraw(renderer, textures)
        .draw({HudText{.text = "SCORE",
                       .position = {30, 40},
                       .size = 24,
                       .color = {.r = 1, .g = 2, .b = 3, .a = 4}}});

    EXPECT_EQ(renderer.text_positions.front(), (Vec2{30, 40}));
    EXPECT_FLOAT_EQ(renderer.text_sizes.front(), 24.F);
    EXPECT_EQ(renderer.text_colors.front(), (Color{.r = 1, .g = 2, .b = 3, .a = 4}));
}

TEST(HudDraw, SpriteIsScaledFromItsTopLeftCornerAndRotatedAroundItsCenter) {
    RecordingRenderer renderer;
    TextureLibrary textures(renderer, "assets");

    HudDraw(renderer, textures)
        .draw({HudSprite{.texture = "ui/heart.png",
                         .position = {100, 50},
                         .scale = {-2, 2},
                         .rotation = 45,
                         .color = {}}});

    ASSERT_EQ(renderer.sprites.size(), 1U);
    const SpriteDraw& drawn = renderer.sprites.front();
    EXPECT_EQ(drawn.texture, heart);
    EXPECT_EQ(drawn.source, (Rect{.x = 0, .y = 0, .width = -16, .height = 8}));
    EXPECT_EQ(drawn.dest, (Rect{.x = 116, .y = 58, .width = 32, .height = 16}));
    EXPECT_EQ(drawn.origin, (Vec2{16, 8}));
    EXPECT_FLOAT_EQ(drawn.rotation, 45.F);
}

TEST(HudDraw, SpriteTexturesAreLoadedOnceFromTheLibrary) {
    RecordingRenderer renderer;
    TextureLibrary textures(renderer, "assets");
    HudDraw hud(renderer, textures);
    const HudSprite sprite{
        .texture = "ui/heart.png", .position = {}, .scale = {1, 1}, .rotation = 0, .color = {}};

    hud.draw({sprite, sprite});
    hud.draw({sprite});

    EXPECT_EQ(renderer.loaded.size(), 1U);
    EXPECT_EQ(renderer.sprites.size(), 3U);
}
