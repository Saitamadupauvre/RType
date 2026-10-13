#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "engine/network/Export.hpp"

#include "Protocol.hpp"

namespace engine::network {

struct ENGINE_NETWORK_EXPORT DecodedPacket {
    PacketHeader header;
    std::vector<std::uint8_t> payload;
};

class ENGINE_NETWORK_EXPORT ProtocolHandler {
public:
    using SerializedData = std::optional<std::vector<std::uint8_t>>;

    [[nodiscard]] SerializedData serialize(const PacketHeader& header,
                                           std::span<const std::uint8_t> payload) const;
    [[nodiscard]] std::optional<DecodedPacket>
    deserialize(std::span<const std::uint8_t> datagram) const;
    [[nodiscard]] SerializedData serialize_input_payload(const InputPayload& payload) const;
    [[nodiscard]] std::optional<InputPayload>
    deserialize_input_payload(std::span<const std::uint8_t> payload) const;

private:
    [[nodiscard]] SerializedData serialize_header(const PacketHeader& header) const;
    [[nodiscard]] std::optional<PacketHeader>
    deserialize_header(std::span<const std::uint8_t> data) const;
};

} // namespace engine::network
