#include "TemporaryNetworkView.hpp"

namespace rtype::client {

namespace {

constexpr float square_size = 64.F;
constexpr float outline_margin = 6.F;
constexpr engine::graphics::Color outline_color{.r = 255, .g = 255, .b = 255, .a = 255};

engine::graphics::Color to_color(engine::core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

bool is_local(const engine::network::EntitySnapshot& entity, std::uint32_t local_player) {
    return entity.owner != 0 && entity.owner == local_player;
}

} // namespace

void temporary_draw_snapshot(engine::graphics::IRenderer& renderer,
                             const std::vector<engine::network::EntitySnapshot>& entities,
                             std::uint32_t local_player) {
    for (const auto& entity : entities) {
        if (is_local(entity, local_player)) {
            renderer.draw_rect({.x = entity.x - outline_margin,
                                .y = entity.y - outline_margin,
                                .width = square_size + (2 * outline_margin),
                                .height = square_size + (2 * outline_margin)},
                               outline_color, false);
        }
        renderer.draw_rect(
            {.x = entity.x, .y = entity.y, .width = square_size, .height = square_size},
            to_color(entity.color));
    }
}

} // namespace rtype::client
