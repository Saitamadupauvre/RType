#pragma once

#include <string>

#include "engine/core/Export.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::core {

/**
 * @brief Point of view of a scene, attached to an entity whose Position is the world point shown
 * at the center of the screen.
 *
 * @note A zero bounds size means the camera is not bounded.
 */
struct Camera {
    /** @brief Scale from world units to screen units; strictly positive. */
    float zoom{1.F};
    /** @brief Tag of the entity the camera centers on; empty to stay where it is. */
    std::string follow;
    /** @brief Top-left corner of the world area the view never leaves. */
    Vec2 bounds_origin;
    /** @brief Size of the world area the view never leaves; zero for no bounds. */
    Vec2 bounds_size;

    /** @return true when bounds_size is positive on both axes. */
    [[nodiscard]] bool bounded() const noexcept {
        return bounds_size.x > 0.F && bounds_size.y > 0.F;
    }
};

/** @brief Resolved view used to draw: the world point at the center of the screen and a zoom. */
struct CameraView {
    Vec2 center;
    /** @brief Strictly positive scale from world units to screen units. */
    float zoom{1.F};

    bool operator==(const CameraView&) const = default;
};

/**
 * @return The view showing world coordinates as screen coordinates: centered on the middle of
 * @p viewport with a zoom of 1.
 */
[[nodiscard]] ENGINE_CORE_EXPORT CameraView default_view(Vec2 viewport) noexcept;

/**
 * @brief Converts a world point to screen units.
 *
 * @param viewport Size of the screen in screen units.
 * @note @p view must have a strictly positive zoom.
 */
[[nodiscard]] ENGINE_CORE_EXPORT Vec2 world_to_screen(const CameraView& view, Vec2 viewport,
                                                      Vec2 point) noexcept;

/**
 * @brief Converts a screen point to world units; the inverse of world_to_screen().
 *
 * @note @p view must have a strictly positive zoom.
 */
[[nodiscard]] ENGINE_CORE_EXPORT Vec2 screen_to_world(const CameraView& view, Vec2 viewport,
                                                      Vec2 point) noexcept;

/**
 * @brief Moves a view so that the visible world area stays inside a rectangle.
 *
 * On each axis where the visible area is larger than the rectangle, the view is centered on the
 * rectangle instead.
 *
 * @param origin, size Top-left corner and size of the rectangle, in world units.
 * @note @p view must have a strictly positive zoom.
 */
[[nodiscard]] ENGINE_CORE_EXPORT CameraView clamp_to_bounds(CameraView view, Vec2 viewport,
                                                            Vec2 origin, Vec2 size) noexcept;

/**
 * @brief Builds the view of a camera entity.
 *
 * @param center Position of the camera entity.
 * @return A view with the camera's zoom, clamped to its bounds when it is bounded.
 */
[[nodiscard]] ENGINE_CORE_EXPORT CameraView camera_view(Vec2 center, const Camera& camera,
                                                        Vec2 viewport) noexcept;

} // namespace engine::core
