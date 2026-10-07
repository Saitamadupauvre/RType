#pragma once

#include <string>

#include "engine/network/Export.hpp"

namespace engine::network {

struct ENGINE_NETWORK_EXPORT Input {
    std::string action;
    float value{};
};

} // namespace engine::network
