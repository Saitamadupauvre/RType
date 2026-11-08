#pragma once

#include <clay.h>

#include <cstdint>
#include <optional>

#include "engine/core/math/Vec2.hpp"
#include "viewport/SceneItems.hpp"
#include "viewport/ViewCamera.hpp"

namespace editor {

/** @brief The selection handles: scale at the bottom-right corner, rotate above the top. */
enum class Handle : std::uint8_t { scale, rotate };

/** @brief Half the size of a handle on screen, in pixels. */
inline constexpr float handle_radius = 6.F;
/** @brief How far above the selection the rotate handle sits on screen, in pixels. */
inline constexpr float rotate_handle_gap = 24.F;
/** @brief The step rotations snap to, in degrees. */
inline constexpr float rotation_step = 15.F;

/** @brief Where a handle of `item` is drawn, in window pixels. */
[[nodiscard]] engine::core::Vec2 handle_position(const SceneItem& item, Handle handle,
                                                 const ViewCamera& camera,
                                                 Clay_BoundingBox viewport);

/** @brief The handle of `item` under `pointer`, if the item can be edited that way. */
[[nodiscard]] std::optional<Handle> handle_at(const SceneItem& item, engine::core::Vec2 pointer,
                                              const ViewCamera& camera, Clay_BoundingBox viewport);

/** @brief The size of `item` at a scale of 1; zero on an axis whose scale is 0. */
[[nodiscard]] engine::core::Vec2 unscaled_size(const SceneItem& item);

/**
 * @brief The scale that puts the bottom-right corner of `item` at `world`, keeping each axis'
 * sign; never smaller than 1% on an axis.
 */
[[nodiscard]] engine::core::Vec2 scale_for(const SceneItem& item, engine::core::Vec2 world);

/**
 * @brief The rotation, in degrees, that points the top of `item` at `world`, snapped to
 * rotation_step unless `snap` is false.
 */
[[nodiscard]] float rotation_for(const SceneItem& item, engine::core::Vec2 world, bool snap);

} // namespace editor
