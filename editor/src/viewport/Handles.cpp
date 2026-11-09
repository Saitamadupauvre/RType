#include "viewport/Handles.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace editor {

namespace {

using engine::core::Vec2;

constexpr float min_scale = 0.01F;
constexpr float degrees_per_radian = 180.F / std::numbers::pi_v<float>;
constexpr float quarter_turn = 90.F;

Vec2 center_of(const SceneItem& item) { return item.top_left + (item.size / 2.F); }

float sign_of(float value) { return value < 0.F ? -1.F : 1.F; }

float axis_scale(float extent, float base, float current) {
    if (base <= 0.F) {
        return current;
    }
    return sign_of(current) * std::max(extent / base, min_scale);
}

float base_extent(float size, float scale) { return scale == 0.F ? 0.F : size / std::abs(scale); }

} // namespace

Vec2 handle_position(const SceneItem& item, Handle handle, const ViewCamera& camera,
                     Clay_BoundingBox viewport) {
    if (handle == Handle::scale) {
        return to_screen(camera, viewport, item.top_left + item.size);
    }
    const Vec2 top = to_screen(camera, viewport, {center_of(item).x, item.top_left.y});
    return {top.x, top.y - rotate_handle_gap};
}

std::optional<Handle> handle_at(const SceneItem& item, Vec2 pointer, const ViewCamera& camera,
                                Clay_BoundingBox viewport) {
    for (const Handle handle : {Handle::scale, Handle::rotate}) {
        const bool editable = handle == Handle::scale ? item.scales : item.rotates;
        const Vec2 position = handle_position(item, handle, camera, viewport);
        if (editable && std::abs(pointer.x - position.x) <= handle_radius &&
            std::abs(pointer.y - position.y) <= handle_radius) {
            return handle;
        }
    }
    return std::nullopt;
}

Vec2 unscaled_size(const SceneItem& item) {
    return {base_extent(item.size.x, item.scale.x), base_extent(item.size.y, item.scale.y)};
}

Vec2 scale_for(const SceneItem& item, Vec2 world) {
    const Vec2 base = unscaled_size(item);
    const Vec2 extent = world - item.top_left;
    return {axis_scale(extent.x, base.x, item.scale.x), axis_scale(extent.y, base.y, item.scale.y)};
}

float rotation_for(const SceneItem& item, Vec2 world, bool snap) {
    const Vec2 offset = world - center_of(item);
    const float degrees = (std::atan2(offset.y, offset.x) * degrees_per_radian) + quarter_turn;
    const float turned = std::fmod(degrees + 360.F, 360.F);
    return snap ? std::fmod(std::round(turned / rotation_step) * rotation_step, 360.F) : turned;
}

} // namespace editor
