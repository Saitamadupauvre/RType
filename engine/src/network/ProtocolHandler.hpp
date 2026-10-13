#pragma once

#include <optional>
#include <span>

#include "engine/network/Export.hpp"

#include "Protocol.hpp"

namespace engine::network {

struct ENGINE_NETWORK_EXPORT DecodedPacket {
    PacketHeader header;
    Bytes payload;
};

class ENGINE_NETWORK_EXPORT ProtocolHandler {
public:
    using SerializedData = std::optional<Bytes>;

    [[nodiscard]] SerializedData serialize(const PacketHeader& header,
                                           std::span<const std::uint8_t> payload) const;
    [[nodiscard]] std::optional<DecodedPacket>
    deserialize(std::span<const std::uint8_t> datagram) const;
};

} // namespace engine::network
