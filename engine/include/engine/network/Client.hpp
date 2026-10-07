#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/network/Export.hpp"

namespace engine::network {

class ENGINE_NETWORK_EXPORT NetworkClient {
public:
    NetworkClient(std::string_view server_address, std::uint16_t port);

    [[nodiscard]] std::string_view server_address() const noexcept;
    [[nodiscard]] std::uint16_t port() const noexcept;

private:
    std::string server_address_;
    std::uint16_t port_;
};

} // namespace engine::network
