#include <gtest/gtest.h>

#include "viewport/ViewCamera.hpp"

using editor::ViewCamera;
using engine::core::Vec2;

namespace {

constexpr Clay_BoundingBox viewport{.x = 100.F, .y = 50.F, .width = 800.F, .height = 600.F};

void expect_near(Vec2 actual, Vec2 expected) {
    EXPECT_NEAR(actual.x, expected.x, 1e-3F);
    EXPECT_NEAR(actual.y, expected.y, 1e-3F);
}

} // namespace

TEST(ViewCamera, StartsWithTheWorldOriginAtTheTopLeft) {
    const ViewCamera camera = editor::initial_camera(viewport);

    expect_near(editor::to_screen(camera, viewport, {0.F, 0.F}), {100.F, 50.F});
    EXPECT_FLOAT_EQ(camera.zoom, 1.F);
}

TEST(ViewCamera, WorldToScreenAndBackIsTheIdentity) {
    const ViewCamera camera{.center = {37.F, -12.F}, .zoom = 2.5F};
    const Vec2 world{-80.F, 410.F};

    expect_near(editor::to_world(camera, viewport, editor::to_screen(camera, viewport, world)),
                world);
}

TEST(ViewCamera, ZoomScalesDistancesOnScreen) {
    const ViewCamera camera{.center = {0.F, 0.F}, .zoom = 2.F};
    const Vec2 origin = editor::to_screen(camera, viewport, {0.F, 0.F});
    const Vec2 ten = editor::to_screen(camera, viewport, {10.F, 0.F});

    EXPECT_FLOAT_EQ(ten.x - origin.x, 20.F);
}

TEST(ViewCamera, PanFollowsThePointer) {
    const ViewCamera camera{.center = {0.F, 0.F}, .zoom = 2.F};
    const Vec2 before = editor::to_screen(camera, viewport, {5.F, 5.F});
    const ViewCamera moved = editor::panned(camera, {30.F, -10.F});

    expect_near(editor::to_screen(moved, viewport, {5.F, 5.F}), before + Vec2{30.F, -10.F});
}

TEST(ViewCamera, ZoomKeepsThePointUnderTheCursor) {
    const ViewCamera camera{.center = {20.F, 40.F}, .zoom = 1.F};
    const Vec2 cursor{300.F, 200.F};
    const Vec2 anchor = editor::to_world(camera, viewport, cursor);
    const ViewCamera closer = editor::zoomed(camera, viewport, cursor, 3.F);

    EXPECT_GT(closer.zoom, camera.zoom);
    expect_near(editor::to_world(closer, viewport, cursor), anchor);
}

TEST(ViewCamera, ZoomStaysWithinItsLimits) {
    const ViewCamera camera{.center = {0.F, 0.F}, .zoom = 1.F};

    EXPECT_FLOAT_EQ(editor::zoomed(camera, viewport, {}, 500.F).zoom, editor::max_zoom);
    EXPECT_FLOAT_EQ(editor::zoomed(camera, viewport, {}, -500.F).zoom, editor::min_zoom);
}
