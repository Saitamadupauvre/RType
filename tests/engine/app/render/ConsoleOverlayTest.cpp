#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../../graphics/fixtures/NullRenderer.hpp"
#include "engine/core/console/CVars.hpp"
#include "engine/core/console/Console.hpp"
#include "render/ConsoleOverlay.hpp"

using engine::app::draw_console;
using engine::core::Vec2;
using engine::core::console::Console;
using engine::core::console::CVars;
using engine::core::console::LineKind;
using engine::graphics::Color;
using engine::graphics::Rect;

namespace {

constexpr float glyph_width = 10.F;

struct DrawnText {
    std::string text;
    Vec2 position;
    Color color;
};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    void draw_rect(const Rect& rect, Color, bool) override { rects.push_back(rect); }
    void draw_text(const std::string& text, Vec2 position, float, Color color) override {
        texts.push_back({.text = text, .position = position, .color = color});
    }
    void draw_sprite(const engine::graphics::SpriteDraw&) override { ++sprites; }
    void begin_world(const engine::core::CameraView&) override { ++world_passes; }
    [[nodiscard]] Vec2 measure_text(const std::string& text, float size) const override {
        return {static_cast<float>(text.size()) * glyph_width, size};
    }

    std::vector<Rect> rects;
    std::vector<DrawnText> texts;
    int sprites{0};
    int world_passes{0};
};

class ConsoleOverlayTest : public ::testing::Test {
protected:
    CVars cvars;
    Console console{cvars};
    RecordingRenderer renderer;
};

} // namespace

TEST_F(ConsoleOverlayTest, DrawsAPanelOverTheTopOfTheScreen) {
    draw_console(renderer, console);

    ASSERT_FALSE(renderer.rects.empty());
    const Rect panel = renderer.rects.front();
    EXPECT_FLOAT_EQ(panel.x, 0.F);
    EXPECT_FLOAT_EQ(panel.y, 0.F);
    EXPECT_FLOAT_EQ(panel.width, renderer.virtual_size().x);
    EXPECT_GT(panel.height, 0.F);
    EXPECT_LT(panel.height, renderer.virtual_size().y);
    EXPECT_EQ(renderer.sprites, 0);
    EXPECT_EQ(renderer.world_passes, 0);
}

TEST_F(ConsoleOverlayTest, DrawsTheInputLastWithACursorAtItsPosition) {
    console.type("abcd");
    console.move_left();

    draw_console(renderer, console);

    ASSERT_FALSE(renderer.texts.empty());
    EXPECT_EQ(renderer.texts.back().text, "> abcd");
    const Rect cursor = renderer.rects.back();
    EXPECT_FLOAT_EQ(cursor.x, renderer.texts.back().position.x + (5 * glyph_width));
    EXPECT_FLOAT_EQ(cursor.y, renderer.texts.back().position.y);
}

TEST_F(ConsoleOverlayTest, DrawsTheNewestLinesAboveTheInputWithTheirColor) {
    console.print(LineKind::output, "old");
    console.print(LineKind::error, "new");

    draw_console(renderer, console);

    ASSERT_EQ(renderer.texts.size(), 3U);
    EXPECT_EQ(renderer.texts[0].text, "old");
    EXPECT_EQ(renderer.texts[1].text, "new");
    EXPECT_LT(renderer.texts[0].position.y, renderer.texts[1].position.y);
    EXPECT_LT(renderer.texts[1].position.y, renderer.texts[2].position.y);
    EXPECT_NE(renderer.texts[0].color, renderer.texts[1].color);
    EXPECT_GT(renderer.texts[1].color.r, renderer.texts[1].color.g);
}

TEST_F(ConsoleOverlayTest, OnlyDrawsTheLinesThatFitInThePanel) {
    for (int line = 0; line < 200; ++line) {
        console.print(LineKind::log, std::to_string(line));
    }

    draw_console(renderer, console);

    const Rect panel = renderer.rects.front();
    ASSERT_GT(renderer.texts.size(), 2U);
    EXPECT_LT(renderer.texts.size(), 200U);
    EXPECT_EQ(renderer.texts[renderer.texts.size() - 2].text, "199");
    for (const auto& text : renderer.texts) {
        EXPECT_GE(text.position.y, panel.y);
        EXPECT_LT(text.position.y, panel.y + panel.height);
    }
}

TEST_F(ConsoleOverlayTest, ScrollingShowsOlderLines) {
    for (int line = 0; line < 200; ++line) {
        console.print(LineKind::log, std::to_string(line));
    }
    console.scroll(10);

    draw_console(renderer, console);

    EXPECT_EQ(renderer.texts[renderer.texts.size() - 2].text, "189");
}
