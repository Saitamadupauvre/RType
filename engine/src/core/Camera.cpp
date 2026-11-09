#include "engine/core/math/Camera.hpp"

#include <algorithm>

namespace engine::core {

namespace {

float clamp_axis(float center, float visible, float origin, float size) {
    if (visible >= size) {
        return origin + (size / 2.F);
    }
    const float half = visible / 2.F;
    return std::clamp(center, origin + half, origin + size - half);
}

} // namespace

CameraView default_view(Vec2 viewport) noexcept { return {.center = viewport / 2.F, .zoom = 1.F}; }

Vec2 world_to_screen(const CameraView& view, Vec2 viewport, Vec2 point) noexcept {
    return ((point - view.center) * view.zoom) + (viewport / 2.F);
}

Vec2 screen_to_world(const CameraView& view, Vec2 viewport, Vec2 point) noexcept {
    return ((point - (viewport / 2.F)) / view.zoom) + view.center;
}

CameraView clamp_to_bounds(CameraView view, Vec2 viewport, Vec2 origin, Vec2 size) noexcept {
    const Vec2 visible = viewport / view.zoom;
    view.center = {clamp_axis(view.center.x, visible.x, origin.x, size.x),
                   clamp_axis(view.center.y, visible.y, origin.y, size.y)};
    return view;
}

CameraView camera_view(Vec2 center, const Camera& camera, Vec2 viewport) noexcept {
    const CameraView view{.center = center, .zoom = camera.zoom};
    if (!camera.bounded()) {
        return view;
    }
    return clamp_to_bounds(view, viewport, camera.bounds_origin, camera.bounds_size);
}

} // namespace engine::core
