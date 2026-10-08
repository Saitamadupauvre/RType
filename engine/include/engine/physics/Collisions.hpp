#pragma once

#include "engine/core/Vec2.hpp"
#include "engine/physics/Export.hpp"

namespace engine::physics {

ENGINE_PHYSICS_EXPORT bool check_aabb_collision(
    const core::Vec2& pos_a, const core::Vec2& size_a,
    const core::Vec2& pos_b, const core::Vec2& size_b,
    core::Vec2& out_normal, float& out_p_depth);

ENGINE_PHYSICS_EXPORT bool check_circle_collision(
    const core::Vec2& pos_a, float radius_a,
    const core::Vec2& pos_b, float radius_b,
    core::Vec2& out_normal, float& out_p_depth);

ENGINE_PHYSICS_EXPORT bool check_aabb_circle_collision(
    const core::Vec2& aabb_pos, const core::Vec2& aabb_size,
    const core::Vec2& circle_pos, float circle_radius,
    core::Vec2& out_normal, float& out_p_depth);

} // namespace engine::physics
