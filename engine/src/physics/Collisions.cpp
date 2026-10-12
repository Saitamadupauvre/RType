#include "engine/physics/Collisions.hpp"

#include <algorithm>
#include <cmath>

namespace engine::physics {

bool check_aabb_collision(const core::Vec2& pos_a, const core::Vec2& size_a,
                          const core::Vec2& pos_b, const core::Vec2& size_b, core::Vec2& out_normal,
                          float& out_p_depth) {
    engine::core::Vec2 a_min = pos_a - size_a / 2.0f;
    engine::core::Vec2 a_max = pos_a + size_a / 2.0f;
    engine::core::Vec2 b_min = pos_b - size_b / 2.0f;
    engine::core::Vec2 b_max = pos_b + size_b / 2.0f;

    if (a_max.x < b_min.x || a_min.x > b_max.x || a_max.y < b_min.y || a_min.y > b_max.y) {
        return false;
    }

    engine::core::Vec2 a_center = pos_a;
    engine::core::Vec2 b_center = pos_b;
    engine::core::Vec2 delta = b_center - a_center;
    float dx =
        (a_max.x - b_min.x) < (b_max.x - a_min.x) ? (a_max.x - b_min.x) : (b_max.x - a_min.x);
    float dy =
        (a_max.y - b_min.y) < (b_max.y - a_min.y) ? (a_max.y - b_min.y) : (b_max.y - a_min.y);
    if (dx < dy) {
        out_normal = engine::core::Vec2{(delta.x > 0) ? 1.0f : -1.0f, 0.0f};
        out_p_depth = dx;
    } else {
        out_normal = engine::core::Vec2{0.0f, (delta.y > 0) ? 1.0f : -1.0f};
        out_p_depth = dy;
    }
    return true;
}

bool check_circle_collision(const core::Vec2& pos_a, float radius_a, const core::Vec2& pos_b,
                            float radius_b, core::Vec2& out_normal, float& out_p_depth) {
    engine::core::Vec2 delta = pos_b - pos_a;
    float dist_sq = delta.x * delta.x + delta.y * delta.y;
    float radius_sum = radius_a + radius_b;
    if (dist_sq > radius_sum * radius_sum)
        return false;

    float dist = std::sqrt(dist_sq);
    if (dist < 0.0001f) {
        out_normal = engine::core::Vec2{1.0f, 0.0f};
        out_p_depth = radius_sum;
    } else {
        out_normal = engine::core::Vec2{delta.x / dist, delta.y / dist};
        out_p_depth = radius_sum - dist;
    }
    return true;
}

bool check_aabb_circle_collision(const core::Vec2& aabb_pos, const core::Vec2& aabb_size,
                                 const core::Vec2& circle_pos, float circle_radius,
                                 core::Vec2& out_normal, float& out_p_depth) {
    engine::core::Vec2 closest;
    closest.x =
        std::clamp(circle_pos.x, aabb_pos.x - aabb_size.x / 2.0f, aabb_pos.x + aabb_size.x / 2.0f);
    closest.y =
        std::clamp(circle_pos.y, aabb_pos.y - aabb_size.y / 2.0f, aabb_pos.y + aabb_size.y / 2.0f);

    engine::core::Vec2 delta = circle_pos - closest;
    float dist_sq = delta.x * delta.x + delta.y * delta.y;
    if (dist_sq > circle_radius * circle_radius)
        return false;

    float dist = std::sqrt(dist_sq);
    if (dist < 0.0001f) {
        engine::core::Vec2 center_delta = circle_pos - aabb_pos;
        float dx = aabb_size.x / 2.0f - std::abs(center_delta.x);
        float dy = aabb_size.y / 2.0f - std::abs(center_delta.y);
        if (dx < dy) {
            out_normal = engine::core::Vec2{(center_delta.x > 0) ? 1.0f : -1.0f, 0.0f};
            out_p_depth = circle_radius + dx;
        } else {
            out_normal = engine::core::Vec2{0.0f, (center_delta.y > 0) ? 1.0f : -1.0f};
            out_p_depth = circle_radius + dy;
        }
    } else {
        out_normal = engine::core::Vec2{delta.x / dist, delta.y / dist};
        out_p_depth = circle_radius - dist;
    }
    return true;
}

} // namespace engine::physics