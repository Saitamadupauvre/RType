#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadSamples.hpp"
#include "encoder/PayloadEncoder.hpp"
#include "protocol/ProtocolHandler.hpp"

namespace {

using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::EntityState;
using engine::network::EventPayload;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::Message;
using engine::network::MessageType;
using engine::network::ProtocolHandler;
using engine::network::SnapshotPayload;
using engine::network::SpawnEvent;

constexpr std::size_t header_type_offset = 6;

template <typename T> T value_of(std::optional<T> value) {
    EXPECT_TRUE(value.has_value());
    return std::move(value).value_or(T{});
}

std::vector<std::pair<Message, MessageType>> sample_messages() {
    return {
        {ConnectRequestPayload{.client_nonce = 0xCAFE'F00D}, MessageType::connect_request},
        {ConnectAcceptPayload{
             .client_nonce = 0xCAFE'F00D,
             .player_entity_id = 12,
             .strings = payload_samples::strings_of(
                 {{.category = engine::network::StringCategory::type, .id = 1, .name = "player"}})},
         MessageType::connect_accept},
        {InputPayload{.action_flags = 0x01, .move_x = -1, .move_y = 1}, MessageType::input},
        {SnapshotPayload{.server_tick = 99,
                         .entities = {EntityState{.entity_id = 12, .type_id = 1, .x = 10}}},
         MessageType::snapshot},
        {EventPayload{SpawnEvent{.entity_id = 12, .type_id = 1, .x = 10, .y = 20}},
         MessageType::event},
        {HeartbeatPayload{}, MessageType::heartbeat},
        {DisconnectPayload{.reason = DisconnectReason::timeout}, MessageType::disconnect},
    };
}

std::vector<std::uint8_t> packet_of(MessageType type, const std::vector<std::uint8_t>& payload) {
    const ProtocolHandler handler;
    const engine::network::PacketHeader header{
        .magic = engine::network::protocol_magic,
        .version = engine::network::protocol_version,
        .type = type,
        .sequence = 1,
        .payload_size = static_cast<std::uint16_t>(payload.size()),
    };
    return handler.serialize(header, payload).value_or(std::vector<std::uint8_t>{});
}

engine::network::PacketHeader input_header(std::uint32_t sequence, std::uint16_t payload_size) {
    return {
        .magic = engine::network::protocol_magic,
        .version = engine::network::protocol_version,
        .type = engine::network::MessageType::input,
        .sequence = sequence,
        .payload_size = payload_size,
    };
}

} // namespace

TEST(ProtocolHandler, SerializesPacketInBigEndianOrder) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 3> payload{1, 0, 1};

    const auto packet = handler.serialize(input_header(0x0102'0304, payload.size()), payload);

    const std::vector<std::uint8_t> expected{0x52, 0x54, 0x59, 0x50, 0x00, 0x04, 0x03, 0x01,
                                             0x02, 0x03, 0x04, 0x00, 0x03, 0x01, 0x00, 0x01};

    EXPECT_EQ(packet, expected);
}

TEST(ProtocolHandler, DeserializesValidPacket) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 3> payload{1, 0, 1};
    const auto encoded = value_of(handler.serialize(input_header(42, payload.size()), payload));

    const auto packet = value_of(handler.deserialize(encoded));

    EXPECT_EQ(packet.header.sequence, 42);
    EXPECT_EQ(packet.payload, std::vector<std::uint8_t>(payload.begin(), payload.end()));
}

TEST(ProtocolHandler, RejectsMalformedPacket) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 13> packet{0xA7, 0x13, 0xD4, 0x6B, 0x00, 0x02, 0xFF,
                                              0x80, 0x35, 0x9A, 0x47, 0x00, 0x00};

    EXPECT_FALSE(handler.deserialize(packet).has_value());
}

TEST(ProtocolHandler, RejectsTruncatedPacket) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 12> packet{};

    EXPECT_FALSE(handler.deserialize(packet).has_value());
}

TEST(ProtocolHandler, RejectsMismatchedPayloadSize) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 14> packet{0x52, 0x54, 0x59, 0x50, 0x00, 0x01, 0x03,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01};

    EXPECT_FALSE(handler.deserialize(packet).has_value());
}

TEST(ProtocolHandler, RoundTripsEveryMessageType) {
    const ProtocolHandler handler;

    for (const auto& [message, type] : sample_messages()) {
        const auto datagram = value_of(handler.encode(message, 42));

        const auto received = value_of(handler.decode(datagram));

        EXPECT_EQ(received.message, message) << "index " << message.index();
        EXPECT_EQ(received.header.type, type);
        EXPECT_EQ(received.header.sequence, 42U);
    }
}

TEST(ProtocolHandler, EncodedHeaderTypeMatchesPayload) {
    const ProtocolHandler handler;

    for (const auto& [message, type] : sample_messages()) {
        const auto datagram = value_of(handler.encode(message, 0));

        ASSERT_GT(datagram.size(), header_type_offset) << "index " << message.index();
        EXPECT_EQ(datagram[header_type_offset], static_cast<std::uint8_t>(type));
    }
}

TEST(ProtocolHandler, EncodeRejectsInvalidPayload) {
    const ProtocolHandler handler;
    const DisconnectPayload undefined_reason{.reason = static_cast<DisconnectReason>(9)};

    EXPECT_FALSE(handler.encode(InputPayload{.move_x = 2}, 0).has_value());
    EXPECT_FALSE(handler.encode(undefined_reason, 0).has_value());
}

TEST(ProtocolHandler, DecodeRejectsPayloadOfAnotherMessageType) {
    const ProtocolHandler handler;
    const InputPayload input{.action_flags = 0x01};
    const ConnectRequestPayload request{.client_nonce = 7};
    const DisconnectPayload disconnect{.reason = DisconnectReason::normal};
    const auto input_bytes = value_of(engine::network::encode_payload(input));
    const auto request_bytes = value_of(engine::network::encode_payload(request));
    const auto disconnect_bytes = value_of(engine::network::encode_payload(disconnect));

    EXPECT_FALSE(handler.decode(packet_of(MessageType::disconnect, input_bytes)).has_value());
    EXPECT_FALSE(handler.decode(packet_of(MessageType::heartbeat, request_bytes)).has_value());
    EXPECT_FALSE(handler.decode(packet_of(MessageType::input, disconnect_bytes)).has_value());
    EXPECT_FALSE(handler.decode(packet_of(MessageType::snapshot, input_bytes)).has_value());
    EXPECT_FALSE(handler.decode(packet_of(MessageType::event, request_bytes)).has_value());
}

TEST(ProtocolHandler, DecodeRejectsInvalidHeader) {
    const ProtocolHandler handler;
    auto datagram = value_of(handler.encode(HeartbeatPayload{}, 0));
    ASSERT_FALSE(datagram.empty());
    datagram.front() = 0x00;

    EXPECT_FALSE(handler.decode(datagram).has_value());
}
