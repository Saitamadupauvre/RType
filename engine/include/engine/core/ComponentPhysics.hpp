#pragma once

#include "engine/core/Vec2.hpp"

namespace engine::core {

struct Position {
    Vec2 pos;

    constexpr Position() noexcept = default;
    constexpr Position(Vec2 pos_) noexcept : pos(pos_) {}
    constexpr Position(float x, float y) noexcept : pos(x, y) {}
};

struct Velocity {
    Vec2 vel;

    constexpr Velocity() noexcept = default;
    constexpr Velocity(Vec2 vel_) noexcept : vel(vel_) {}
    constexpr Velocity(float x, float y) noexcept : vel(x, y) {}
};

} // namespace engine::core
