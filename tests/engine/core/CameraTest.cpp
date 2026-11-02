#include <gtest/gtest.h>

#include <limits>

#include "engine/core/Camera.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"

using engine::core::Camera;
using engine::core::camera_view;
using engine::core::CameraView;
using engine::core::clamp_to_bounds;
using engine::core::default_view;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldFlags;
using engine::core::FieldKind;
using engine::core::FieldRef;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::FieldValueOutOfRange;
using engine::core::Position;
using engine::core::screen_to_world;
using engine::core::Vec2;
using engine::core::world_to_screen;

namespace {

constexpr Vec2 viewport{1920.F, 1080.F};

void expect_near(Vec2 actual, Vec2 expected) {
    EXPECT_NEAR(actual.x, expected.x, 1e-3F);
    EXPECT_NEAR(actual.y, expected.y, 1e-3F);
}

} // namespace

TEST(CameraView, DefaultViewCentersOnViewportWithoutZoom) {
    EXPECT_EQ(default_view(viewport), (CameraView{.center = {960.F, 540.F}, .zoom = 1.F}));
}

TEST(CameraView, DefaultViewMapsWorldToSameScreenPoint) {
    const CameraView view = default_view(viewport);
    for (const Vec2 point : {Vec2{0.F, 0.F}, Vec2{1920.F, 1080.F}, Vec2{-30.F, 712.5F}}) {
        EXPECT_EQ(world_to_screen(view, viewport, point), point);
        EXPECT_EQ(screen_to_world(view, viewport, point), point);
    }
}

TEST(CameraView, OffsetShiftsWorldOppositeToCamera) {
    const CameraView view{.center = {1060.F, 490.F}, .zoom = 1.F};
    EXPECT_EQ(world_to_screen(view, viewport, {960.F, 540.F}), (Vec2{860.F, 590.F}));
    EXPECT_EQ(screen_to_world(view, viewport, {960.F, 540.F}), (Vec2{1060.F, 490.F}));
}

TEST(CameraView, ZoomScalesAroundScreenCenter) {
    const CameraView view{.center = {100.F, 100.F}, .zoom = 2.F};
    EXPECT_EQ(world_to_screen(view, viewport, {100.F, 100.F}), (Vec2{960.F, 540.F}));
    EXPECT_EQ(world_to_screen(view, viewport, {110.F, 90.F}), (Vec2{980.F, 520.F}));
    EXPECT_EQ(screen_to_world(view, viewport, {0.F, 0.F}), (Vec2{-380.F, -170.F}));
}

TEST(CameraView, ScreenToWorldInvertsWorldToScreen) {
    const CameraView view{.center = {-250.F, 3000.F}, .zoom = 0.75F};
    for (const Vec2 point : {Vec2{0.F, 0.F}, Vec2{12.F, -7.5F}, Vec2{4000.F, 2500.F}}) {
        expect_near(screen_to_world(view, viewport, world_to_screen(view, viewport, point)), point);
    }
}

TEST(CameraBounds, KeepsViewInsideLargerBounds) {
    const Vec2 origin{0.F, 0.F};
    const Vec2 size{5000.F, 2000.F};
    EXPECT_EQ(clamp_to_bounds({.center = {100.F, 100.F}, .zoom = 1.F}, viewport, origin, size),
              (CameraView{.center = {960.F, 540.F}, .zoom = 1.F}));
    EXPECT_EQ(clamp_to_bounds({.center = {4900.F, 1900.F}, .zoom = 1.F}, viewport, origin, size),
              (CameraView{.center = {4040.F, 1460.F}, .zoom = 1.F}));
    EXPECT_EQ(clamp_to_bounds({.center = {2500.F, 1000.F}, .zoom = 1.F}, viewport, origin, size),
              (CameraView{.center = {2500.F, 1000.F}, .zoom = 1.F}));
}

TEST(CameraBounds, ZoomShrinksVisibleArea) {
    const CameraView clamped = clamp_to_bounds({.center = {0.F, 0.F}, .zoom = 2.F}, viewport,
                                               {0.F, 0.F}, {5000.F, 2000.F});
    EXPECT_EQ(clamped.center, (Vec2{480.F, 270.F}));
    EXPECT_EQ(clamped.zoom, 2.F);
}

TEST(CameraBounds, CentersOnBoundsSmallerThanView) {
    const CameraView clamped = clamp_to_bounds({.center = {9000.F, 50.F}, .zoom = 1.F}, viewport,
                                               {100.F, 0.F}, {800.F, 2000.F});
    EXPECT_EQ(clamped.center, (Vec2{500.F, 540.F}));
}

TEST(CameraBounds, CameraViewIgnoresBoundsWhenUnbounded) {
    const Camera camera{.zoom = 1.5F, .follow = {}, .bounds_origin = {}, .bounds_size = {}};
    EXPECT_FALSE(camera.bounded());
    EXPECT_EQ(camera_view({-500.F, -500.F}, camera, viewport),
              (CameraView{.center = {-500.F, -500.F}, .zoom = 1.5F}));
}

TEST(CameraBounds, CameraViewClampsWhenBounded) {
    const Camera camera{
        .zoom = 1.F, .follow = {}, .bounds_origin = {0.F, 0.F}, .bounds_size = {4000.F, 1080.F}};
    EXPECT_TRUE(camera.bounded());
    EXPECT_EQ(camera_view({-500.F, 900.F}, camera, viewport),
              (CameraView{.center = {960.F, 540.F}, .zoom = 1.F}));
}

TEST(CameraBounds, ZeroOnOneAxisMeansUnbounded) {
    const Camera camera{
        .zoom = 1.F, .follow = {}, .bounds_origin = {}, .bounds_size = {4000.F, 0.F}};
    EXPECT_FALSE(camera.bounded());
}

class CameraFieldsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine::core::register_core_fields(registry);
        world.register_component<Position>();
        world.register_component<Camera>();
        entity = world.spawn();
    }

    FieldRegistry registry;
    EntityManager world;
    Entity entity{0, 0};
};

TEST_F(CameraFieldsTest, RegistersFieldsWithKindsAndFlags) {
    const FieldRef zoom = registry.find_field("Camera", "zoom");
    const FieldRef follow = registry.find_field("Camera", "follow");
    const FieldRef origin = registry.find_field("Camera", "bounds_origin");
    const FieldRef size = registry.find_field("Camera", "bounds_size");
    ASSERT_TRUE(zoom && follow && origin && size);
    EXPECT_EQ(zoom.field->kind(), FieldKind::Float);
    EXPECT_EQ(follow.field->kind(), FieldKind::String);
    EXPECT_EQ(origin.field->kind(), FieldKind::Vec2);
    EXPECT_EQ(size.field->kind(), FieldKind::Vec2);
    EXPECT_EQ(zoom.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
    EXPECT_EQ(follow.field->flags(), (FieldFlags{.replicated = false, .editable = true}));
    EXPECT_EQ(origin.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
    EXPECT_EQ(size.field->flags(), (FieldFlags{.replicated = true, .editable = true}));
}

TEST_F(CameraFieldsTest, SettingFieldGivesEntityAPosition) {
    registry.set(world, entity, "Camera", "zoom", 2.F);
    EXPECT_EQ(world.get<Camera>(entity).zoom, 2.F);
    EXPECT_TRUE(world.has<Position>(entity));
}

TEST_F(CameraFieldsTest, ReadsAndWritesEveryField) {
    registry.set(world, entity, "Camera", "follow", std::string{"player"});
    registry.set(world, entity, "Camera", "bounds_origin", Vec2{10.F, 20.F});
    registry.set(world, entity, "Camera", "bounds_size", Vec2{3000.F, 1080.F});
    EXPECT_EQ(registry.get(world, entity, "Camera", "follow"), FieldValue{std::string{"player"}});
    EXPECT_EQ(registry.get(world, entity, "Camera", "zoom"), FieldValue{1.F});
    EXPECT_EQ(world.get<Camera>(entity).bounds_origin, (Vec2{10.F, 20.F}));
    EXPECT_EQ(world.get<Camera>(entity).bounds_size, (Vec2{3000.F, 1080.F}));
}

TEST_F(CameraFieldsTest, RejectsZoomThatIsNotPositive) {
    for (const float zoom : {0.F, -1.F, std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_THROW(registry.set(world, entity, "Camera", "zoom", zoom), FieldValueOutOfRange);
    }
    EXPECT_FALSE(world.has<Camera>(entity));
}

TEST_F(CameraFieldsTest, RejectsNegativeOrInfiniteBounds) {
    EXPECT_THROW(registry.set(world, entity, "Camera", "bounds_size", Vec2{-1.F, 10.F}),
                 FieldValueOutOfRange);
    EXPECT_THROW(registry.set(world, entity, "Camera", "bounds_origin",
                              Vec2{std::numeric_limits<float>::infinity(), 0.F}),
                 FieldValueOutOfRange);
    EXPECT_FALSE(world.has<Camera>(entity));
}

TEST_F(CameraFieldsTest, ZeroBoundsSizeIsAccepted) {
    registry.set(world, entity, "Camera", "bounds_size", Vec2{0.F, 0.F});
    EXPECT_FALSE(world.get<Camera>(entity).bounded());
}
