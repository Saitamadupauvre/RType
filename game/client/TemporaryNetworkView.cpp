#include "TemporaryNetworkView.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace rtype::client {

namespace {

constexpr std::uint8_t fire_flag = 1U;
constexpr float axis_range = 127.F;
constexpr float square_size = 64.F;
constexpr engine::graphics::Color square_color{.r = 255, .g = 255, .b = 255, .a = 255};

std::int8_t to_network_axis(const engine::script::InputState& state, std::string_view axis) {
    const auto found = state.axes.find(axis);
    if (found == state.axes.end()) {
        return 0;
    }
    return static_cast<std::int8_t>(std::lround(std::clamp(found->second, -1.F, 1.F) * axis_range));
}

} // namespace

TemporaryNetworkInput temporary_to_network_input(const engine::script::InputState& state) {
    return {.action_flags = state.pressed.contains("fire") ? fire_flag : std::uint8_t{0},
            .move_x = to_network_axis(state, "move_x"),
            .move_y = to_network_axis(state, "move_y")};
}

void temporary_draw_snapshot(engine::graphics::IRenderer& renderer,
                             const std::vector<engine::network::EntitySnapshot>& entities) {
    for (const auto& entity : entities) {
        renderer.draw_rect(
            {.x = entity.x, .y = entity.y, .width = square_size, .height = square_size},
            square_color);
    }
}

} // namespace rtype::client
