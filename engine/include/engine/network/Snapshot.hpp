#pragma once

#include <cstdint>
#include <vector>

#include "engine/network/Export.hpp"

namespace engine::network {

struct ENGINE_NETWORK_EXPORT Snapshot {
    std::uint32_t sequence{};
    std::vector<std::uint8_t> data;
};

} // namespace engine::network
