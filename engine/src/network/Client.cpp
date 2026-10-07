#include "engine/network/Client.hpp"

#include <utility>

namespace engine::network {

NetworkClient::NetworkClient(std::string_view server_address, std::uint16_t port)
    : server_address_(server_address), port_(port) {}

std::string_view NetworkClient::server_address() const noexcept { return server_address_; }

std::uint16_t NetworkClient::port() const noexcept { return port_; }

} // namespace engine::network
