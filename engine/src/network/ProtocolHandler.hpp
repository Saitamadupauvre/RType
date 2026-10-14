#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <variant>

#include "engine/network/Export.hpp"

#include "Protocol.hpp"

namespace engine::network {

using Message = std::variant<ConnectRequestPayload, ConnectAcceptPayload, InputPayload,
                             SnapshotPayload, EventPayload, HeartbeatPayload, DisconnectPayload>;

struct ENGINE_NETWORK_EXPORT DecodedPacket {
    PacketHeader header;
    Bytes payload;
};

struct ENGINE_NETWORK_EXPORT ReceivedMessage {
    PacketHeader header;
    Message message;
};

class ENGINE_NETWORK_EXPORT ProtocolHandler {
public:
    using SerializedData = std::optional<Bytes>;

    [[nodiscard]] SerializedData serialize(const PacketHeader& header,
                                           std::span<const std::uint8_t> payload) const;
    [[nodiscard]] std::optional<DecodedPacket>
    deserialize(std::span<const std::uint8_t> datagram) const;

    [[nodiscard]] SerializedData encode(const Message& message, std::uint32_t sequence) const;
    [[nodiscard]] std::optional<ReceivedMessage>
    decode(std::span<const std::uint8_t> datagram) const;
};

} // namespace engine::network
