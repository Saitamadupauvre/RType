#pragma once

#include <cmath>
#include <ostream>

namespace engine::core {

/**
 * @brief A 2D vector with x and y components.
 */
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() noexcept = default;
    constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& other) const noexcept {
        return Vec2{x + other.x, y + other.y};
    }

    [[nodiscard]] constexpr Vec2 operator-(const Vec2& other) const noexcept {
        return Vec2{x - other.x, y - other.y};
    }

    [[nodiscard]] constexpr Vec2 operator*(float scalar) const noexcept {
        return Vec2{x * scalar, y * scalar};
    }

    [[nodiscard]] constexpr bool operator==(const Vec2& other) const noexcept {
        return x == other.x && y == other.y;
    }

    [[nodiscard]] constexpr bool operator!=(const Vec2& other) const noexcept {
        return x != other.x || y != other.y;
    }

    [[nodiscard]] float length() const noexcept { return std::sqrt(x * x + y * y); }

    [[nodiscard]] Vec2 normalize() const noexcept {
        float len = length();
        if (len == 0.0f) {
            return Vec2{0.0f, 0.0f};
        }
        return Vec2{x / len, y / len};
    }
};

inline std::ostream& operator<<(std::ostream& os, const Vec2& vec) {
    os << "Vec2(" << vec.x << ", " << vec.y << ")";
    return os;
}

} // namespace engine::core