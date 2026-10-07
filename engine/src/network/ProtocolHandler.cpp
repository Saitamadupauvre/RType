#include "ProtocolHandler.hpp"

namespace engine::network {

namespace {

bool is_valid_message_type(std::uint8_t value) {
    return value >= static_cast<std::uint8_t>(MessageType::connect_request) &&
           value <= static_cast<std::uint8_t>(MessageType::disconnect);
}

bool is_valid_input_payload(const InputPayload& payload) {
    return (payload.action_flags & 0xFE) == 0 && payload.move_x >= -1 && payload.move_x <= 1 &&
           payload.move_y >= -1 && payload.move_y <= 1;
}

void append_u16(std::vector<std::uint8_t>& data, std::uint16_t value) {
    data.push_back(static_cast<std::uint8_t>(value >> 8));
    data.push_back(static_cast<std::uint8_t>(value));
}

void append_u32(std::vector<std::uint8_t>& data, std::uint32_t value) {
    data.push_back(static_cast<std::uint8_t>(value >> 24));
    data.push_back(static_cast<std::uint8_t>(value >> 16));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
    data.push_back(static_cast<std::uint8_t>(value));
}

std::uint16_t read_u16(std::span<const std::uint8_t> data, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[offset]) << 8) |
                                      data[offset + 1]);
}

std::uint32_t read_u32(std::span<const std::uint8_t> data, std::size_t offset) {
    return (static_cast<std::uint32_t>(data[offset]) << 24) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
}

} // namespace

ProtocolHandler::SerializedData
ProtocolHandler::serialize(const PacketHeader& header,
                           std::span<const std::uint8_t> payload) const {
    if (payload.size() > max_payload_size || header.payload_size != payload.size()) {
        return std::nullopt;
    }

    auto data = serialize_header(header);
    if (!data.has_value()) {
        return std::nullopt;
    }
    data->insert(data->end(), payload.begin(), payload.end());
    return data;
}

std::optional<DecodedPacket>
ProtocolHandler::deserialize(std::span<const std::uint8_t> datagram) const {
    const auto header = deserialize_header(datagram);
    if (!header.has_value() ||
        datagram.size() != encoded_packet_header_size + header->payload_size) {
        return std::nullopt;
    }

    return DecodedPacket{
        .header = *header,
        .payload = std::vector<std::uint8_t>(datagram.begin() + encoded_packet_header_size,
                                             datagram.end()),
    };
}

ProtocolHandler::SerializedData
ProtocolHandler::serialize_input_payload(const InputPayload& payload) const {
    if (!is_valid_input_payload(payload)) {
        return std::nullopt;
    }

    return std::vector<std::uint8_t>{
        payload.action_flags,
        static_cast<std::uint8_t>(payload.move_x),
        static_cast<std::uint8_t>(payload.move_y),
    };
}

std::optional<InputPayload>
ProtocolHandler::deserialize_input_payload(std::span<const std::uint8_t> payload) const {
    if (payload.size() != 3) {
        return std::nullopt;
    }

    const InputPayload input{
        .action_flags = payload[0],
        .move_x = static_cast<std::int8_t>(payload[1]),
        .move_y = static_cast<std::int8_t>(payload[2]),
    };
    if (!is_valid_input_payload(input)) {
        return std::nullopt;
    }
    return input;
}

ProtocolHandler::SerializedData
ProtocolHandler::serialize_header(const PacketHeader& header) const {
    if (header.magic != protocol_magic || header.version != protocol_version ||
        !is_valid_message_type(static_cast<std::uint8_t>(header.type)) ||
        header.payload_size > max_payload_size) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> data;
    data.reserve(encoded_packet_header_size);
    append_u32(data, header.magic);
    append_u16(data, header.version);
    data.push_back(static_cast<std::uint8_t>(header.type));
    append_u32(data, header.sequence);
    append_u16(data, header.payload_size);
    return data;
}

std::optional<PacketHeader>
ProtocolHandler::deserialize_header(std::span<const std::uint8_t> data) const {
    if (data.size() < encoded_packet_header_size) {
        return std::nullopt;
    }

    const PacketHeader header{
        .magic = read_u32(data, 0),
        .version = read_u16(data, 4),
        .type = static_cast<MessageType>(data[6]),
        .sequence = read_u32(data, 7),
        .payload_size = read_u16(data, 11),
    };
    if (header.magic != protocol_magic || header.version != protocol_version ||
        !is_valid_message_type(static_cast<std::uint8_t>(header.type)) ||
        header.payload_size > max_payload_size) {
        return std::nullopt;
    }
    return header;
}

} // namespace engine::network
