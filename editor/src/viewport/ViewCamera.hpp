#pragma once

#include <clay.h>

#include "engine/core/math/Vec2.hpp"

namespace editor {

/** @brief The editor's own view of the scene, separate from the game camera. */
struct ViewCamera {
    /** @brief World point shown at the middle of the viewport. */
    engine::core::Vec2 center;
    /** @brief Viewport pixels per world unit, between min_zoom and max_zoom. */
    float zoom = 1.F;

    bool operator==(const ViewCamera&) const = default;
};

inline constexpr float min_zoom = 0.1F;
inline constexpr float max_zoom = 10.F;
inline constexpr float zoom_per_notch = 1.1F;

/** @brief The camera showing the world origin at the top-left corner of the viewport, at zoom 1. */
[[nodiscard]] ViewCamera initial_camera(Clay_BoundingBox viewport);

/** @brief Converts a world point to window pixels, for a viewport placed at `viewport`. */
[[nodiscard]] engine::core::Vec2 to_screen(const ViewCamera& camera, Clay_BoundingBox viewport,
                                           engine::core::Vec2 world);

/** @brief Converts window pixels to a world point; the inverse of to_screen(). */
[[nodiscard]] engine::core::Vec2 to_world(const ViewCamera& camera, Clay_BoundingBox viewport,
                                          engine::core::Vec2 screen);

/** @brief Moves the view so the world follows the pointer by `screen_delta` pixels. */
[[nodiscard]] ViewCamera panned(ViewCamera camera, engine::core::Vec2 screen_delta);

/** @brief Zooms by `notches` wheel notches, keeping the world point under `cursor` in place. */
[[nodiscard]] ViewCamera zoomed(ViewCamera camera, Clay_BoundingBox viewport,
                                engine::core::Vec2 cursor, float notches);

} // namespace editor
