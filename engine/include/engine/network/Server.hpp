#pragma once

#include "engine/network/Export.hpp"

#include <cstdint>

namespace engine::network {

class ENGINE_NETWORK_EXPORT NetworkServer {

public:
    explicit NetworkServer(std::uint16_t port) noexcept;

    [[nodiscard]] std::uint16_t port() const noexcept;

private:
    std::uint16_t port_;
};

} // namespace engine::network
