#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "TemporaryNetworkView.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"

using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::TextureId;
using engine::network::EntitySnapshot;
using rtype::client::temporary_draw_snapshot;

namespace {

struct DrawnRect {
    Rect rect;
    Color color;
    bool filled;
};

class RecordingRenderer final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return false; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] Vec2 virtual_size() const override { return {1920, 1080}; }
    [[nodiscard]] TextureId load_texture(const std::string&) override { return {}; }
    void unload_texture(TextureId) override {}
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {}; }
    void begin_frame(Color) override {}
    void end_frame() override {}
    void draw_sprite(const engine::graphics::SpriteDraw&) override {}
    void draw_rect(const Rect& rect, Color color, bool filled) override {
        rects.push_back({.rect = rect, .color = color, .filled = filled});
    }
    void draw_circle(Vec2, float, Color) override {}
    void draw_line(Vec2, Vec2, float, Color) override {}
    void draw_text(const std::string&, Vec2, float, Color) override {}
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {}; }

    std::vector<DrawnRect> rects;
};

EntitySnapshot square(float x, std::uint8_t owner) {
    return {.entity_id = 1,
            .type_id = 1,
            .x = x,
            .y = 20.F,
            .vx = 0.F,
            .vy = 0.F,
            .owner = owner,
            .color = {.r = 10, .g = 20, .b = 30, .a = 255}};
}

} // namespace

TEST(TemporaryNetworkView, DrawsEachEntityWithItsColor) {
    RecordingRenderer renderer;

    temporary_draw_snapshot(renderer, {square(5.F, 2)}, 1);

    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.rects.front().rect.x, 5.F);
    EXPECT_EQ(renderer.rects.front().color, (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
    EXPECT_TRUE(renderer.rects.front().filled);
}

TEST(TemporaryNetworkView, OutlinesTheLocalPlayerEntity) {
    RecordingRenderer renderer;

    temporary_draw_snapshot(renderer, {square(5.F, 1), square(200.F, 2)}, 1);

    ASSERT_EQ(renderer.rects.size(), 3U);
    EXPECT_FALSE(renderer.rects.front().filled);
    EXPECT_LT(renderer.rects.front().rect.x, 5.F);
}

TEST(TemporaryNetworkView, UnownedEntitiesAreNeverOutlinedBeforeAccept) {
    RecordingRenderer renderer;

    temporary_draw_snapshot(renderer, {square(5.F, 0)}, 0);

    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_TRUE(renderer.rects.front().filled);
}
