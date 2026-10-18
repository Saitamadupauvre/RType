#include "DatagramHandler.hpp"

#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::network {

std::optional<Inbound> handle_datagram(const ProtocolHandler& handler,
                                       std::span<const std::uint8_t> datagram,
                                       const Endpoint& from) {
    auto received = handler.decode(datagram);
    if (!received) {
        core::log::warn("network: dropped invalid datagram of {} bytes from {}", datagram.size(),
                        from.to_string());
        return std::nullopt;
    }
    return Inbound{.from = from, .message = std::move(*received)};
}

std::optional<Bytes> encode_outbound(const ProtocolHandler& handler, const Outbound& outbound,
                                     std::uint32_t sequence) {
    auto bytes = handler.encode(outbound.message, sequence);
    if (!bytes) {
        core::log::warn("network: dropped invalid outbound message to {}", outbound.to.to_string());
    }
    return bytes;
}

} // namespace engine::network
