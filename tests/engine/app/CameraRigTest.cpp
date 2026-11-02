#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "CameraRig.hpp"
#include "Fields.hpp"
#include "engine/core/Camera.hpp"
#include "engine/core/Entity.hpp"
#include "engine/network/Client.hpp"
#include "engine/script/ClientApi.hpp"

using engine::app::is_camera;
using engine::app::resolve_camera;
using engine::core::CameraView;
using engine::core::Entity;
using engine::core::FieldRegistry;
using engine::core::FieldValue;
using engine::core::Vec2;
using engine::network::EntitySnapshot;
using engine::network::ReplicatedValue;
using engine::script::CameraOverride;

namespace {

constexpr Vec2 viewport{1920.F, 1080.F};
const FieldRegistry fields = engine::app::replicated_fields();

ReplicatedValue value_of(std::string_view component, std::string_view field, FieldValue value) {
    return {.field = fields.find_field(component, field), .value = std::move(value)};
}

EntitySnapshot ship(std::uint32_t id, Vec2 position) {
    return {.entity_id = id, .type_id = 1, .fields = {value_of("Position", "pos", position)}};
}

EntitySnapshot camera(std::uint32_t id, Vec2 position, float zoom, Vec2 bounds_origin = {},
                      Vec2 bounds_size = {}) {
    return {.entity_id = id,
            .type_id = 2,
            .fields = {value_of("Position", "pos", position), value_of("Camera", "zoom", zoom),
                       value_of("Camera", "bounds_origin", bounds_origin),
                       value_of("Camera", "bounds_size", bounds_size)}};
}

CameraOverride fixed(Vec2 center, float zoom) {
    return {
        .kind = CameraOverride::Kind::Fixed, .view = {.center = center, .zoom = zoom}, .entity = 0};
}

CameraOverride following(Entity entity, float zoom) {
    return {.kind = CameraOverride::Kind::Follow,
            .view = {.center = {}, .zoom = zoom},
            .entity = entity.id()};
}

} // namespace

TEST(CameraRig, NoCameraKeepsTheDefaultView) {
    EXPECT_EQ(resolve_camera({ship(1, {10.F, 10.F})}, {}, viewport),
              engine::core::default_view(viewport));
    EXPECT_EQ(resolve_camera({}, {}, viewport), engine::core::default_view(viewport));
}

TEST(CameraRig, RecognisesCameraEntities) {
    EXPECT_TRUE(is_camera(camera(1, {}, 1.F)));
    EXPECT_FALSE(is_camera(ship(1, {})));
}

TEST(CameraRig, SceneCameraIsUsedByDefault) {
    EXPECT_EQ(resolve_camera({ship(1, {}), camera(2, {300.F, 400.F}, 2.F)}, {}, viewport),
              (CameraView{.center = {300.F, 400.F}, .zoom = 2.F}));
}

TEST(CameraRig, LowestEntityIdWinsAmongSceneCameras) {
    EXPECT_EQ(
        resolve_camera({camera(9, {1.F, 1.F}, 3.F), camera(4, {2.F, 2.F}, 2.F)}, {}, viewport),
        (CameraView{.center = {2.F, 2.F}, .zoom = 2.F}));
}

TEST(CameraRig, SceneCameraIsClampedToItsBounds) {
    const auto view =
        resolve_camera({camera(1, {0.F, 0.F}, 1.F, {0.F, 0.F}, {4000.F, 1080.F})}, {}, viewport);
    EXPECT_EQ(view, (CameraView{.center = {960.F, 540.F}, .zoom = 1.F}));
}

TEST(CameraRig, InvalidReplicatedZoomFallsBackToOne) {
    EXPECT_FLOAT_EQ(resolve_camera({camera(1, {}, 0.F)}, {}, viewport).zoom, 1.F);
    EXPECT_FLOAT_EQ(
        resolve_camera({camera(1, {}, std::numeric_limits<float>::quiet_NaN())}, {}, viewport).zoom,
        1.F);
}

TEST(CameraRig, FixedOverrideWinsOverSceneCamera) {
    EXPECT_EQ(resolve_camera({camera(1, {300.F, 400.F}, 2.F)}, fixed({5.F, 6.F}, 0.5F), viewport),
              (CameraView{.center = {5.F, 6.F}, .zoom = 0.5F}));
}

TEST(CameraRig, FollowOverrideCentersOnTheEntityWithTheSameIndex) {
    const Entity target{7, 3};

    EXPECT_EQ(resolve_camera({ship(7, {120.F, 80.F})}, following(target, 1.5F), viewport),
              (CameraView{.center = {120.F, 80.F}, .zoom = 1.5F}));
}

TEST(CameraRig, FollowOverrideKeepsSceneBounds) {
    const Entity target{7, 0};
    const auto view =
        resolve_camera({camera(1, {}, 1.F, {0.F, 0.F}, {4000.F, 1080.F}), ship(7, {10.F, 10.F})},
                       following(target, 1.F), viewport);
    EXPECT_EQ(view, (CameraView{.center = {960.F, 540.F}, .zoom = 1.F}));
}

TEST(CameraRig, FollowOverrideOnMissingEntityFallsBackToScene) {
    EXPECT_EQ(
        resolve_camera({camera(1, {300.F, 400.F}, 2.F)}, following(Entity{42, 0}, 1.F), viewport),
        (CameraView{.center = {300.F, 400.F}, .zoom = 2.F}));
    EXPECT_EQ(resolve_camera({}, following(Entity{42, 0}, 1.F), viewport),
              engine::core::default_view(viewport));
}
