#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/Vec2.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace rtype::client {

struct Star {
    engine::core::Vec2 position;
    float speed;
    float size;
    engine::graphics::Color color;
};

class Starfield {
public:
    Starfield(std::uint32_t seed, engine::core::Vec2 area);

    void update(float dt);
    void draw(engine::graphics::IRenderer& renderer) const;

    [[nodiscard]] const std::vector<Star>& stars() const;

private:
    engine::core::Vec2 _area;
    std::vector<Star> _stars;
};

} // namespace rtype::client
