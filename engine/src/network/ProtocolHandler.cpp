#include "ProtocolHandler.hpp"

#include <algorithm>
#include <array>
#include <utility>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"
#include "Payloads.hpp"

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

MessageType message_type_of(const ConnectRequestPayload&) noexcept {
    return MessageType::connect_request;
}

MessageType message_type_of(const ConnectAcceptPayload&) noexcept {
    return MessageType::connect_accept;
}

MessageType message_type_of(const InputPayload&) noexcept { return MessageType::input; }

MessageType message_type_of(const SnapshotPayload&) noexcept { return MessageType::snapshot; }

MessageType message_type_of(const EventPayload&) noexcept { return MessageType::event; }

MessageType message_type_of(const HeartbeatPayload&) noexcept { return MessageType::heartbeat; }

MessageType message_type_of(const DisconnectPayload&) noexcept { return MessageType::disconnect; }

template <auto Decode> std::optional<Message> decode_as(std::span<const std::uint8_t> payload) {
    auto decoded = Decode(payload);
    if (!decoded) {
        return std::nullopt;
    }
    return Message{std::move(*decoded)};
}

using MessageDecoder = std::optional<Message> (*)(std::span<const std::uint8_t>);

struct MessageDecoderEntry {
    MessageType type;
    MessageDecoder decode;
};

constexpr std::array message_decoders{
    MessageDecoderEntry{.type = MessageType::connect_request,
                        .decode = decode_as<decode_connect_request>},
    MessageDecoderEntry{.type = MessageType::connect_accept,
                        .decode = decode_as<decode_connect_accept>},
    MessageDecoderEntry{.type = MessageType::input, .decode = decode_as<decode_input>},
    MessageDecoderEntry{.type = MessageType::snapshot, .decode = decode_as<decode_snapshot>},
    MessageDecoderEntry{.type = MessageType::event, .decode = decode_as<decode_event>},
    MessageDecoderEntry{.type = MessageType::heartbeat, .decode = decode_as<decode_heartbeat>},
    MessageDecoderEntry{.type = MessageType::disconnect, .decode = decode_as<decode_disconnect>},
};

std::optional<Message> decode_message(MessageType type, std::span<const std::uint8_t> payload) {
    const auto entry = std::ranges::find(message_decoders, type, &MessageDecoderEntry::type);
    if (entry == message_decoders.end()) {
        return std::nullopt;
    }
    return entry->decode(payload);
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

ProtocolHandler::SerializedData ProtocolHandler::encode(const Message& message,
                                                        std::uint32_t sequence) const {
    return std::visit(
        [this, sequence](const auto& payload) -> SerializedData {
            const auto bytes = encode_payload(payload);
            if (!bytes) {
                return std::nullopt;
            }

            const PacketHeader header{
                .magic = protocol_magic,
                .version = protocol_version,
                .type = message_type_of(payload),
                .sequence = sequence,
                .payload_size = static_cast<std::uint16_t>(bytes->size()),
            };
            return serialize(header, *bytes);
        },
        message);
}

std::optional<ReceivedMessage>
ProtocolHandler::decode(std::span<const std::uint8_t> datagram) const {
    const auto packet = deserialize(datagram);
    if (!packet) {
        return std::nullopt;
    }

    auto message = decode_message(packet->header.type, packet->payload);
    if (!message) {
        return std::nullopt;
    }
    return ReceivedMessage{.header = packet->header, .message = std::move(*message)};
}

} // namespace engine::network
