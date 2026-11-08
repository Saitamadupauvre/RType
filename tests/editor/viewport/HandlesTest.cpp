#include <gtest/gtest.h>

#include <cmath>
#include <optional>

#include "viewport/Handles.hpp"

using editor::Handle;
using editor::SceneItem;
using engine::core::Vec2;

namespace {

constexpr Clay_BoundingBox viewport{.x = 0.F, .y = 0.F, .width = 800.F, .height = 600.F};

editor::ViewCamera camera() { return editor::initial_camera(viewport); }

SceneItem item() {
    SceneItem shown;
    shown.top_left = {100.F, 100.F};
    shown.size = {64.F, 32.F};
    shown.scale = {2.F, 1.F};
    shown.scales = true;
    shown.rotates = true;
    return shown;
}

} // namespace

TEST(Handles, ScaleHandleIsAtTheBottomRightCorner) {
    EXPECT_EQ(editor::handle_position(item(), Handle::scale, camera(), viewport),
              Vec2(164.F, 132.F));
}

TEST(Handles, RotateHandleIsAboveTheTopMiddle) {
    EXPECT_EQ(editor::handle_position(item(), Handle::rotate, camera(), viewport),
              Vec2(132.F, 100.F - editor::rotate_handle_gap));
}

TEST(Handles, PointerFindsTheHandleWithinItsRadius) {
    EXPECT_EQ(editor::handle_at(item(), {167.F, 129.F}, camera(), viewport), Handle::scale);
    EXPECT_EQ(editor::handle_at(item(), {132.F, 78.F}, camera(), viewport), Handle::rotate);
    EXPECT_EQ(editor::handle_at(item(), {120.F, 120.F}, camera(), viewport), std::nullopt);
}

TEST(Handles, OnlyEditableTransformsHaveHandles) {
    SceneItem fixed = item();
    fixed.scales = false;
    fixed.rotates = false;

    EXPECT_EQ(editor::handle_at(fixed, {164.F, 132.F}, camera(), viewport), std::nullopt);
    EXPECT_EQ(editor::handle_at(fixed, {132.F, 76.F}, camera(), viewport), std::nullopt);
}

TEST(Handles, UnscaledSizeDividesByTheScale) {
    EXPECT_EQ(editor::unscaled_size(item()), Vec2(32.F, 32.F));
}

TEST(Handles, ScalePutsTheCornerUnderThePointer) {
    EXPECT_EQ(editor::scale_for(item(), {196.F, 164.F}), Vec2(3.F, 2.F));
}

TEST(Handles, ScaleKeepsAMirroredAxis) {
    SceneItem mirrored = item();
    mirrored.scale = {-2.F, 1.F};

    EXPECT_EQ(editor::scale_for(mirrored, {196.F, 132.F}), Vec2(-3.F, 1.F));
}

TEST(Handles, ScaleNeverCollapses) {
    const Vec2 scale = editor::scale_for(item(), {0.F, 0.F});

    EXPECT_FLOAT_EQ(scale.x, 0.01F);
    EXPECT_FLOAT_EQ(scale.y, 0.01F);
}

TEST(Handles, RotationPointsTheTopAtThePointer) {
    const SceneItem shown = item();

    EXPECT_FLOAT_EQ(editor::rotation_for(shown, {132.F, 0.F}, false), 0.F);
    EXPECT_FLOAT_EQ(editor::rotation_for(shown, {300.F, 116.F}, false), 90.F);
    EXPECT_FLOAT_EQ(editor::rotation_for(shown, {132.F, 300.F}, false), 180.F);
    EXPECT_FLOAT_EQ(editor::rotation_for(shown, {0.F, 116.F}, false), 270.F);
}

TEST(Handles, RotationSnapsToItsStep) {
    const SceneItem shown = item();
    const float snapped = editor::rotation_for(shown, {300.F, 150.F}, true);

    EXPECT_FLOAT_EQ(std::fmod(snapped, editor::rotation_step), 0.F);
    EXPECT_NE(snapped, editor::rotation_for(shown, {300.F, 150.F}, false));
}
