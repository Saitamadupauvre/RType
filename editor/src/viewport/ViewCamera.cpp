#include "viewport/ViewCamera.hpp"

#include <algorithm>
#include <cmath>

#include "engine/core/math/Camera.hpp"

namespace editor {

namespace {

using engine::core::Vec2;

Vec2 origin_of(Clay_BoundingBox viewport) { return {viewport.x, viewport.y}; }

Vec2 size_of(Clay_BoundingBox viewport) { return {viewport.width, viewport.height}; }

engine::core::CameraView view_of(const ViewCamera& camera) {
    return {.center = camera.center, .zoom = camera.zoom};
}

} // namespace

ViewCamera initial_camera(Clay_BoundingBox viewport) {
    return {.center = size_of(viewport) / 2.F, .zoom = 1.F};
}

Vec2 to_screen(const ViewCamera& camera, Clay_BoundingBox viewport, Vec2 world) {
    return origin_of(viewport) +
           engine::core::world_to_screen(view_of(camera), size_of(viewport), world);
}

Vec2 to_world(const ViewCamera& camera, Clay_BoundingBox viewport, Vec2 screen) {
    return engine::core::screen_to_world(view_of(camera), size_of(viewport),
                                         screen - origin_of(viewport));
}

ViewCamera panned(ViewCamera camera, Vec2 screen_delta) {
    camera.center -= screen_delta / camera.zoom;
    return camera;
}

ViewCamera zoomed(ViewCamera camera, Clay_BoundingBox viewport, Vec2 cursor, float notches) {
    const Vec2 anchor = to_world(camera, viewport, cursor);
    const float zoom = camera.zoom * std::pow(zoom_per_notch, notches);
    camera.zoom = std::clamp(zoom, min_zoom, max_zoom);
    camera.center += anchor - to_world(camera, viewport, cursor);
    return camera;
}

} // namespace editor
