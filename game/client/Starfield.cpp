#include "Starfield.hpp"

#include <array>
#include <random>

namespace rtype::client {

namespace {

struct Layer {
    int count;
    float speed;
    float size;
    std::uint8_t brightness;
};

constexpr std::array layers{
    Layer{.count = 80, .speed = 40.F, .size = 2.F, .brightness = 90},
    Layer{.count = 50, .speed = 100.F, .size = 3.F, .brightness = 160},
    Layer{.count = 25, .speed = 220.F, .size = 4.F, .brightness = 255},
};

} // namespace

Starfield::Starfield(std::uint32_t seed, engine::core::Vec2 area) : _area(area) {
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> x(0.F, area.x);
    std::uniform_real_distribution<float> y(0.F, area.y);
    for (const Layer& layer : layers) {
        const engine::graphics::Color color{
            .r = layer.brightness, .g = layer.brightness, .b = layer.brightness, .a = 255};
        for (int i = 0; i < layer.count; ++i) {
            _stars.push_back({.position = {x(random), y(random)},
                              .speed = layer.speed,
                              .size = layer.size,
                              .color = color});
        }
    }
}

void Starfield::update(float dt) {
    for (Star& star : _stars) {
        star.position.x -= star.speed * dt;
        while (star.position.x < -star.size) {
            star.position.x += _area.x + star.size;
        }
    }
}

void Starfield::draw(engine::graphics::IRenderer& renderer) const {
    for (const Star& star : _stars) {
        renderer.draw_rect(
            {.x = star.position.x, .y = star.position.y, .width = star.size, .height = star.size},
            star.color);
    }
}

const std::vector<Star>& Starfield::stars() const { return _stars; }

} // namespace rtype::client
