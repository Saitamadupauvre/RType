#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "ProtocolHandler.hpp"

namespace {

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

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(*packet, (std::vector<std::uint8_t>{0x52, 0x54, 0x59, 0x50, 0x00, 0x01, 0x03, 0x01,
                                                  0x02, 0x03, 0x04, 0x00, 0x03, 0x01, 0x00, 0x01}));
}

TEST(ProtocolHandler, DeserializesValidPacket) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 3> payload{1, 0, 1};
    const auto encoded = handler.serialize(input_header(42, payload.size()), payload);

    ASSERT_TRUE(encoded.has_value());
    const auto packet = handler.deserialize(*encoded);

    ASSERT_TRUE(packet.has_value());
    EXPECT_EQ(packet->header.sequence, 42);
    EXPECT_EQ(packet->payload, std::vector<std::uint8_t>(payload.begin(), payload.end()));
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

TEST(ProtocolHandler, RejectsInvalidInputPayload) {
    const engine::network::ProtocolHandler handler;
    const engine::network::InputPayload payload{.action_flags = 0x02, .move_x = 2, .move_y = 0};

    EXPECT_FALSE(handler.serialize_input_payload(payload).has_value());
}

TEST(ProtocolHandler, DeserializesValidInputPayload) {
    const engine::network::ProtocolHandler handler;
    const std::array<std::uint8_t, 3> payload{0x01, 0xFF, 0x01};

    const auto input = handler.deserialize_input_payload(payload);

    ASSERT_TRUE(input.has_value());
    EXPECT_EQ(input->action_flags, 0x01);
    EXPECT_EQ(input->move_x, -1);
    EXPECT_EQ(input->move_y, 1);
}
