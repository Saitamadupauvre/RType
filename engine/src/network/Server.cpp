#include "engine/network/Server.hpp"

namespace engine::network {

NetworkServer::NetworkServer(std::uint16_t port) noexcept : port_(port) {}

std::uint16_t NetworkServer::port() const noexcept { return port_; }

} // namespace engine::network
