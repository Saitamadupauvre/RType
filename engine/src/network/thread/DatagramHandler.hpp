#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "engine/network/Export.hpp"

#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkMessages.hpp"

namespace engine::network {

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<Inbound>
handle_datagram(const ProtocolHandler& handler, std::span<const std::uint8_t> datagram,
                const Endpoint& from);

[[nodiscard]] ENGINE_NETWORK_EXPORT std::optional<Bytes>
encode_outbound(const ProtocolHandler& handler, const Outbound& outbound, std::uint32_t sequence);

} // namespace engine::network
