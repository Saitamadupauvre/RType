#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../graphics/fixtures/NullRenderer.hpp"
#include "LoadingScreen.hpp"
#include "engine/graphics/IRenderer.hpp"

using engine::app::draw_loading_screen;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;

namespace {

struct DrawnRect {
    Rect rect;
    bool filled;
};

struct DrawnText {
    std::string text;
    Vec2 position;
};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    [[nodiscard]] Vec2 virtual_size() const override { return {1000, 600}; }
    [[nodiscard]] TextureId load_texture(const std::string&) override {
        ++loads;
        return {};
    }
    void draw_sprite(const SpriteDraw&) override { ++sprites; }
    void draw_rect(const Rect& rect, Color, bool filled) override {
        rects.push_back({.rect = rect, .filled = filled});
    }
    void draw_text(const std::string& text, Vec2 position, float, Color) override {
        texts.push_back({.text = text, .position = position});
    }
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {60, 32}; }

    int loads{0};
    int sprites{0};
    std::vector<DrawnRect> rects;
    std::vector<DrawnText> texts;
};

} // namespace

TEST(LoadingScreen, DrawsACenteredBarFilledWithTheProgress) {
    RecordingRenderer renderer;

    draw_loading_screen(renderer, 3, 4);

    ASSERT_EQ(renderer.rects.size(), 2U);
    EXPECT_FALSE(renderer.rects[0].filled);
    EXPECT_EQ(renderer.rects[0].rect, (Rect{.x = 250, .y = 288, .width = 500, .height = 24}));
    EXPECT_TRUE(renderer.rects[1].filled);
    EXPECT_EQ(renderer.rects[1].rect, (Rect{.x = 250, .y = 288, .width = 375, .height = 24}));
}

TEST(LoadingScreen, WritesTheCountCenteredAboveTheBar) {
    RecordingRenderer renderer;

    draw_loading_screen(renderer, 3, 4);

    ASSERT_EQ(renderer.texts.size(), 1U);
    EXPECT_EQ(renderer.texts[0].text, "3 / 4");
    EXPECT_FLOAT_EQ(renderer.texts[0].position.x, 470.F);
    EXPECT_FLOAT_EQ(renderer.texts[0].position.y, 240.F);
}

TEST(LoadingScreen, NothingToLoadShowsAnEmptyBar) {
    RecordingRenderer renderer;

    draw_loading_screen(renderer, 0, 0);

    ASSERT_EQ(renderer.rects.size(), 2U);
    EXPECT_FLOAT_EQ(renderer.rects[1].rect.width, 0.F);
    EXPECT_EQ(renderer.texts[0].text, "0 / 0");
}

TEST(LoadingScreen, NeedsNoAsset) {
    RecordingRenderer renderer;

    draw_loading_screen(renderer, 1, 2);

    EXPECT_EQ(renderer.loads, 0);
    EXPECT_EQ(renderer.sprites, 0);
}
