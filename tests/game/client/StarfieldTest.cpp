#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "Starfield.hpp"
#include "engine/graphics/IRenderer.hpp"

using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using rtype::client::Star;
using rtype::client::Starfield;

namespace {

constexpr Vec2 area{1920, 1080};

class RectCounter final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig&) override { return true; }
    void close() override {}
    [[nodiscard]] bool should_close() const override { return false; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] Vec2 virtual_size() const override { return area; }
    [[nodiscard]] TextureId load_texture(const std::string&) override { return {}; }
    void unload_texture(TextureId) override {}
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {}; }
    void begin_frame(Color) override {}
    void end_frame() override {}
    void draw_sprite(const SpriteDraw&) override {}
    void draw_rect(const Rect& rect, Color, bool) override { rects.push_back(rect); }
    void draw_circle(Vec2, float, Color) override {}
    void draw_line(Vec2, Vec2, float, Color) override {}
    void draw_text(const std::string&, Vec2, float, Color) override {}
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {}; }

    std::vector<Rect> rects;
};

bool same_positions(const std::vector<Star>& left, const std::vector<Star>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].position != right[i].position) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(Starfield, SameSeedGivesSameStars) {
    EXPECT_TRUE(same_positions(Starfield(42, area).stars(), Starfield(42, area).stars()));
}

TEST(Starfield, DifferentSeedsGiveDifferentStars) {
    EXPECT_FALSE(same_positions(Starfield(1, area).stars(), Starfield(2, area).stars()));
}

TEST(Starfield, StarsStartInsideTheArea) {
    const Starfield starfield(7, area);

    ASSERT_FALSE(starfield.stars().empty());
    for (const Star& star : starfield.stars()) {
        EXPECT_GE(star.position.x, 0.F);
        EXPECT_LT(star.position.x, area.x);
        EXPECT_GE(star.position.y, 0.F);
        EXPECT_LT(star.position.y, area.y);
    }
}

TEST(Starfield, StarsScrollLeftAtTheirLayerSpeed) {
    Starfield starfield(7, area);
    const std::vector<Star> before = starfield.stars();

    starfield.update(0.001F);

    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i].position.x < 1.F) {
            continue;
        }
        EXPECT_NEAR(starfield.stars()[i].position.x,
                    before[i].position.x - (before[i].speed * 0.001F), 0.001F);
        EXPECT_FLOAT_EQ(starfield.stars()[i].position.y, before[i].position.y);
    }
}

TEST(Starfield, HasLayersOfDifferentSpeeds) {
    const Starfield starfield(7, area);
    float slowest = starfield.stars().front().speed;
    float fastest = slowest;
    for (const Star& star : starfield.stars()) {
        slowest = std::min(slowest, star.speed);
        fastest = std::max(fastest, star.speed);
    }

    EXPECT_LT(slowest, fastest);
}

TEST(Starfield, MovementDoesNotDependOnStepSize) {
    Starfield one_step(7, area);
    Starfield many_steps(7, area);

    one_step.update(1.F);
    for (int i = 0; i < 60; ++i) {
        many_steps.update(1.F / 60.F);
    }

    for (std::size_t i = 0; i < one_step.stars().size(); ++i) {
        EXPECT_NEAR(one_step.stars()[i].position.x, many_steps.stars()[i].position.x, 0.05F);
    }
}

TEST(Starfield, StarsLeavingOnTheLeftComeBackOnTheRight) {
    Starfield starfield(7, area);

    starfield.update(100.F);

    for (const Star& star : starfield.stars()) {
        EXPECT_GE(star.position.x, -star.size);
        EXPECT_LT(star.position.x, area.x);
    }
}

TEST(Starfield, DrawsEveryStarAsASquare) {
    const Starfield starfield(7, area);
    RectCounter renderer;

    starfield.draw(renderer);

    ASSERT_EQ(renderer.rects.size(), starfield.stars().size());
    const Star& first = starfield.stars().front();
    EXPECT_EQ(renderer.rects.front(), (Rect{.x = first.position.x,
                                            .y = first.position.y,
                                            .width = first.size,
                                            .height = first.size}));
}
