#include "ProtocolHandler.hpp"

#include <utility>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"

namespace engine::network {

namespace {

bool is_valid_message_type(MessageType type) noexcept {
    const auto value = static_cast<std::uint8_t>(type);
    return value >= static_cast<std::uint8_t>(MessageType::connect_request) &&
           value <= static_cast<std::uint8_t>(MessageType::disconnect);
}

bool is_valid_header(const PacketHeader& header) noexcept {
    return header.magic == protocol_magic && header.version == protocol_version &&
           is_valid_message_type(header.type) && header.payload_size <= max_payload_size;
}

std::optional<PacketHeader> read_header(ByteReader& reader) noexcept {
    const auto magic = reader.read_u32();
    const auto version = reader.read_u16();
    const auto type = reader.read_u8();
    const auto sequence = reader.read_u32();
    const auto payload_size = reader.read_u16();
    if (!magic || !version || !type || !sequence || !payload_size) {
        return std::nullopt;
    }

    const PacketHeader header{
        .magic = *magic,
        .version = *version,
        .type = static_cast<MessageType>(*type),
        .sequence = *sequence,
        .payload_size = *payload_size,
    };
    if (!is_valid_header(header)) {
        return std::nullopt;
    }
    return header;
}

} // namespace

ProtocolHandler::SerializedData
ProtocolHandler::serialize(const PacketHeader& header,
                           std::span<const std::uint8_t> payload) const {
    if (!is_valid_header(header) || header.payload_size != payload.size()) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u32(header.magic);
    writer.write_u16(header.version);
    writer.write_u8(static_cast<std::uint8_t>(header.type));
    writer.write_u32(header.sequence);
    writer.write_u16(header.payload_size);
    writer.write_bytes(payload);
    return std::move(writer).take();
}

std::optional<DecodedPacket>
ProtocolHandler::deserialize(std::span<const std::uint8_t> datagram) const {
    ByteReader reader(datagram);
    const auto header = read_header(reader);
    if (!header.has_value() || reader.remaining() != header->payload_size) {
        return std::nullopt;
    }

    const auto payload = reader.read_bytes(header->payload_size);
    return DecodedPacket{
        .header = *header,
        .payload = Bytes(payload->begin(), payload->end()),
    };
}

} // namespace engine::network
