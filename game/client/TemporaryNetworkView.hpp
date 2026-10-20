#pragma once

#include <cstdint>
#include <vector>

#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"
#include "engine/script/InputState.hpp"

namespace rtype::client {

struct TemporaryNetworkInput {
    std::uint8_t action_flags;
    std::int8_t move_x;
    std::int8_t move_y;

    bool operator==(const TemporaryNetworkInput&) const = default;
};

TemporaryNetworkInput temporary_to_network_input(const engine::script::InputState& state);

void temporary_draw_snapshot(engine::graphics::IRenderer& renderer,
                             const std::vector<engine::network::EntitySnapshot>& entities);

} // namespace rtype::client
