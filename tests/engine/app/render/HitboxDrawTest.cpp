#include <gtest/gtest.h>

#include <vector>

#include "../../graphics/fixtures/NullRenderer.hpp"
#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/physics/Components.hpp"
#include "render/HitboxDraw.hpp"

using engine::app::draw_hitboxes;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::graphics::Color;
using engine::graphics::Rect;
using engine::physics::AABBCollider;
using engine::physics::CircleCollider;

namespace {

struct DrawnCircle {
    Vec2 center;
    float radius;
};

class RecordingRenderer final : public fixture::NullRenderer {
public:
    void draw_rect(const Rect& rect, Color, bool filled) override {
        rects.push_back(rect);
        outlines.push_back(!filled);
    }
    void draw_circle(Vec2 center, float radius, Color) override {
        circles.push_back({.center = center, .radius = radius});
    }

    std::vector<Rect> rects;
    std::vector<bool> outlines;
    std::vector<DrawnCircle> circles;
};

} // namespace

TEST(HitboxDraw, OutlinesBoxesCenteredOnThePositionPlusOffset) {
    EntityManager world;
    world.register_component<Position>();
    world.register_component<AABBCollider>();
    const auto entity = world.spawn();
    world.add(entity, Position{Vec2{100.F, 50.F}});
    world.add(entity, AABBCollider{.offset = {10.F, 0.F}, .size = {40.F, 20.F}});
    RecordingRenderer renderer;

    draw_hitboxes(renderer, world);

    ASSERT_EQ(renderer.rects.size(), 1U);
    EXPECT_TRUE(renderer.outlines.front());
    EXPECT_FLOAT_EQ(renderer.rects.front().x, 90.F);
    EXPECT_FLOAT_EQ(renderer.rects.front().y, 40.F);
    EXPECT_FLOAT_EQ(renderer.rects.front().width, 40.F);
    EXPECT_FLOAT_EQ(renderer.rects.front().height, 20.F);
}

TEST(HitboxDraw, DrawsCirclesWithTheirRadius) {
    EntityManager world;
    world.register_component<Position>();
    world.register_component<CircleCollider>();
    const auto entity = world.spawn();
    world.add(entity, Position{Vec2{5.F, 5.F}});
    world.add(entity, CircleCollider{.offset = {1.F, 2.F}, .radius = 8.F});
    RecordingRenderer renderer;

    draw_hitboxes(renderer, world);

    ASSERT_EQ(renderer.circles.size(), 1U);
    EXPECT_FLOAT_EQ(renderer.circles.front().center.x, 6.F);
    EXPECT_FLOAT_EQ(renderer.circles.front().center.y, 7.F);
    EXPECT_FLOAT_EQ(renderer.circles.front().radius, 8.F);
}

TEST(HitboxDraw, SkipsWorldsWithoutColliders) {
    EntityManager world;
    world.register_component<Position>();
    world.add(world.spawn(), Position{Vec2{1.F, 1.F}});
    RecordingRenderer renderer;

    draw_hitboxes(renderer, world);
    EntityManager empty;
    draw_hitboxes(renderer, empty);

    EXPECT_TRUE(renderer.rects.empty());
    EXPECT_TRUE(renderer.circles.empty());
}
