#pragma once

#include <cstdint>
#include <vector>

#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"

namespace rtype::client {

void temporary_draw_snapshot(engine::graphics::IRenderer& renderer,
                             const std::vector<engine::network::EntitySnapshot>& entities,
                             std::uint32_t local_player);

} // namespace rtype::client
