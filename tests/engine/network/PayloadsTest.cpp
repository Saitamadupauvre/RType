#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "Payloads.hpp"

using engine::network::ConnectRequestPayload;
using engine::network::decode_connect_request;
using engine::network::decode_disconnect;
using engine::network::decode_heartbeat;
using engine::network::decode_input;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::encode_payload;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;

TEST(InputPayload, EncodesFieldsInOrder) {
    const InputPayload input{.action_flags = 0x01, .move_x = -1, .move_y = 1};

    const auto bytes = encode_payload(input);

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{0x01, 0xFF, 0x01}));
}

TEST(InputPayload, DecodesValidPayload) {
    const std::array<std::uint8_t, 3> payload{0x01, 0xFF, 0x01};

    const auto input = decode_input(payload);

    ASSERT_TRUE(input.has_value());
    EXPECT_EQ(input->action_flags, 0x01);
    EXPECT_EQ(input->move_x, -1);
    EXPECT_EQ(input->move_y, 1);
}

TEST(InputPayload, RoundTripsEveryValidCombination) {
    constexpr std::array<std::uint8_t, 2> flag_values{0x00, 0x01};
    constexpr std::array<std::int8_t, 3> axis_values{-1, 0, 1};

    for (const auto flags : flag_values) {
        for (const auto move_x : axis_values) {
            for (const auto move_y : axis_values) {
                const InputPayload input{.action_flags = flags, .move_x = move_x, .move_y = move_y};

                const auto bytes = encode_payload(input);

                ASSERT_TRUE(bytes.has_value());
                EXPECT_EQ(decode_input(*bytes), input);
            }
        }
    }
}

TEST(InputPayload, EncoderRejectsReservedActionBits) {
    for (unsigned int bit = 1; bit < 8; ++bit) {
        const InputPayload input{.action_flags = static_cast<std::uint8_t>(1U << bit)};

        EXPECT_FALSE(encode_payload(input).has_value()) << "bit " << bit;
    }
}

TEST(InputPayload, EncoderRejectsOutOfRangeMovement) {
    EXPECT_FALSE(encode_payload(InputPayload{.move_x = 2}).has_value());
    EXPECT_FALSE(encode_payload(InputPayload{.move_x = -2}).has_value());
    EXPECT_FALSE(encode_payload(InputPayload{.move_y = 2}).has_value());
    EXPECT_FALSE(encode_payload(InputPayload{.move_y = -128}).has_value());
}

TEST(InputPayload, DecoderRejectsReservedActionBits) {
    for (unsigned int bit = 1; bit < 8; ++bit) {
        const std::array<std::uint8_t, 3> payload{static_cast<std::uint8_t>(1U << bit), 0, 0};

        EXPECT_FALSE(decode_input(payload).has_value()) << "bit " << bit;
    }
}

TEST(InputPayload, DecoderRejectsOutOfRangeMovement) {
    EXPECT_FALSE(decode_input(std::array<std::uint8_t, 3>{0x00, 0x02, 0x00}).has_value());
    EXPECT_FALSE(decode_input(std::array<std::uint8_t, 3>{0x00, 0xFE, 0x00}).has_value());
    EXPECT_FALSE(decode_input(std::array<std::uint8_t, 3>{0x00, 0x00, 0x7F}).has_value());
    EXPECT_FALSE(decode_input(std::array<std::uint8_t, 3>{0x00, 0x00, 0x80}).has_value());
}

TEST(InputPayload, DecoderRejectsTruncatedPayload) {
    EXPECT_FALSE(decode_input(std::vector<std::uint8_t>{}).has_value());
    EXPECT_FALSE(decode_input(std::vector<std::uint8_t>{0x01}).has_value());
    EXPECT_FALSE(decode_input(std::vector<std::uint8_t>{0x01, 0x00}).has_value());
}

TEST(InputPayload, DecoderRejectsTrailingBytes) {
    EXPECT_FALSE(decode_input(std::vector<std::uint8_t>{0x01, 0x00, 0x00, 0x00}).has_value());
}

TEST(ConnectRequestPayload, EncodesNonceInBigEndianOrder) {
    const auto bytes = encode_payload(ConnectRequestPayload{.client_nonce = 0x0102'0304});

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}));
}

TEST(ConnectRequestPayload, RoundTripsBoundaryNonces) {
    constexpr std::array<std::uint32_t, 3> nonces{0x0000'0000, 0x8000'0001, 0xFFFF'FFFF};

    for (const auto nonce : nonces) {
        const ConnectRequestPayload request{.client_nonce = nonce};

        const auto bytes = encode_payload(request);

        ASSERT_TRUE(bytes.has_value());
        EXPECT_EQ(decode_connect_request(*bytes), request);
    }
}

TEST(ConnectRequestPayload, DecoderRejectsTruncatedPayload) {
    const std::vector<std::uint8_t> full{0x01, 0x02, 0x03, 0x04};

    for (std::size_t size = 0; size < full.size(); ++size) {
        const std::vector<std::uint8_t> truncated(full.begin(),
                                                  full.begin() + static_cast<std::ptrdiff_t>(size));

        EXPECT_FALSE(decode_connect_request(truncated).has_value()) << "size " << size;
    }
}

TEST(ConnectRequestPayload, DecoderRejectsTrailingBytes) {
    const std::vector<std::uint8_t> payload{0x01, 0x02, 0x03, 0x04, 0x05};

    EXPECT_FALSE(decode_connect_request(payload).has_value());
}

TEST(HeartbeatPayload, EncodesEmptyPayload) {
    const auto bytes = encode_payload(HeartbeatPayload{});

    ASSERT_TRUE(bytes.has_value());
    EXPECT_TRUE(bytes->empty());
}

TEST(HeartbeatPayload, RoundTrips) {
    const auto bytes = encode_payload(HeartbeatPayload{});

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(decode_heartbeat(*bytes), HeartbeatPayload{});
}

TEST(HeartbeatPayload, DecoderRejectsNonEmptyPayload) {
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x00}).has_value());
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x01, 0x02}).has_value());
}

TEST(DisconnectPayload, EncodesReasonAsOneByte) {
    const DisconnectPayload disconnect{.reason = DisconnectReason::server_shutdown};

    const auto bytes = encode_payload(disconnect);

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, (std::vector<std::uint8_t>{0x02}));
}

TEST(DisconnectPayload, RoundTripsEveryDefinedReason) {
    constexpr std::array<DisconnectReason, 4> reasons{
        DisconnectReason::normal,
        DisconnectReason::timeout,
        DisconnectReason::server_shutdown,
        DisconnectReason::protocol_error,
    };

    for (const auto reason : reasons) {
        const DisconnectPayload disconnect{.reason = reason};

        const auto bytes = encode_payload(disconnect);

        ASSERT_TRUE(bytes.has_value());
        EXPECT_EQ(decode_disconnect(*bytes), disconnect);
    }
}

TEST(DisconnectPayload, EncoderRejectsUndefinedReasons) {
    for (unsigned int value = 4; value <= 255; ++value) {
        const DisconnectPayload disconnect{.reason = static_cast<DisconnectReason>(value)};

        EXPECT_FALSE(encode_payload(disconnect).has_value()) << "reason " << value;
    }
}

TEST(DisconnectPayload, DecoderRejectsUndefinedReasons) {
    for (unsigned int value = 4; value <= 255; ++value) {
        const std::array<std::uint8_t, 1> payload{static_cast<std::uint8_t>(value)};

        EXPECT_FALSE(decode_disconnect(payload).has_value()) << "reason " << value;
    }
}

TEST(DisconnectPayload, DecoderRejectsWrongSize) {
    EXPECT_FALSE(decode_disconnect(std::vector<std::uint8_t>{}).has_value());
    EXPECT_FALSE(decode_disconnect(std::vector<std::uint8_t>{0x00, 0x00}).has_value());
}
