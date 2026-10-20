#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadSamples.hpp"
#include "encoder/PayloadEncoder.hpp"

using engine::network::ConnectRequestPayload;
using engine::network::DestroyEvent;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::encode_payload;
using engine::network::EntityState;
using engine::network::EventPayload;
using engine::network::FireEvent;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::max_payload_size;
using engine::network::max_snapshot_entities;
using engine::network::max_type_name_length;
using engine::network::PlayerLeftEvent;
using engine::network::SnapshotPayload;
using engine::network::SoundEvent;
using engine::network::SpawnEvent;
using engine::network::TypeEntry;
using payload_samples::accept_with;
using payload_samples::encoded;
using payload_samples::long_name_types;
using payload_samples::snapshot_with;

TEST(InputPayload, EncodesFieldsInOrder) {
    const InputPayload input{.action_flags = 0x01, .move_x = -1, .move_y = 1};

    EXPECT_EQ(encode_payload(input), (std::vector<std::uint8_t>{0x01, 0xFF, 0x01}));
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

TEST(ConnectRequestPayload, EncodesNonceInBigEndianOrder) {
    const ConnectRequestPayload request{.client_nonce = 0x0102'0304};

    EXPECT_EQ(encode_payload(request), (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}));
}

TEST(HeartbeatPayload, EncodesEmptyPayload) {
    EXPECT_EQ(encode_payload(HeartbeatPayload{}), std::vector<std::uint8_t>{});
}

TEST(DisconnectPayload, EncodesReasonAsOneByte) {
    const DisconnectPayload disconnect{.reason = DisconnectReason::server_shutdown};

    EXPECT_EQ(encode_payload(disconnect), (std::vector<std::uint8_t>{0x02}));
}

TEST(DisconnectPayload, EncodesServerFullReason) {
    const DisconnectPayload disconnect{.reason = DisconnectReason::server_full};

    EXPECT_EQ(encode_payload(disconnect), (std::vector<std::uint8_t>{0x04}));
}

TEST(DisconnectPayload, EncoderRejectsUndefinedReasons) {
    for (unsigned int value = 5; value <= 255; ++value) {
        const DisconnectPayload disconnect{.reason = static_cast<DisconnectReason>(value)};

        EXPECT_FALSE(encode_payload(disconnect).has_value()) << "reason " << value;
    }
}

TEST(EventPayload, EncodesSpawnFieldsInOrder) {
    const EventPayload event =
        SpawnEvent{.entity_id = 0x0102'0304, .type_id = 0x0506, .x = -2, .y = 0x0708};

    const std::vector<std::uint8_t> expected{0x01, 0x01, 0x02, 0x03, 0x04, 0x05,
                                             0x06, 0xFF, 0xFE, 0x07, 0x08};

    EXPECT_EQ(encoded(event), expected);
}

TEST(EventPayload, EncodesDestroyFieldsInOrder) {
    const EventPayload event = DestroyEvent{.entity_id = 0x0102'0304};

    EXPECT_EQ(encoded(event), (std::vector<std::uint8_t>{0x02, 0x01, 0x02, 0x03, 0x04}));
}

TEST(EventPayload, EncodesFireFieldsInOrder) {
    const EventPayload event =
        FireEvent{.source_entity_id = 0x0102'0304, .projectile_entity_id = 0x0506'0708};

    EXPECT_EQ(encoded(event),
              (std::vector<std::uint8_t>{0x03, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}));
}

TEST(EventPayload, EncodesSoundFieldsInOrder) {
    const EventPayload event = SoundEvent{.source_entity_id = 0x0102'0304, .sound_id = 0x0506};

    EXPECT_EQ(encoded(event),
              (std::vector<std::uint8_t>{0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}));
}

TEST(EventPayload, EncodesPlayerLeftFieldsInOrder) {
    const EventPayload event = PlayerLeftEvent{.player_entity_id = 0x0102'0304};

    EXPECT_EQ(encoded(event), (std::vector<std::uint8_t>{0x05, 0x01, 0x02, 0x03, 0x04}));
}

TEST(SnapshotPayload, EncodesFieldsInOrder) {
    const SnapshotPayload snapshot{
        .server_tick = 0x0102'0304,
        .entities = {EntityState{
            .entity_id = 0x0506'0708,
            .type_id = 0x090A,
            .x = -1,
            .y = 2,
            .vx = -3,
            .vy = 4,
        }},
    };

    const std::vector<std::uint8_t> prefix{0x01, 0x02, 0x03, 0x04, 0x00, 0x01};
    const std::vector<std::uint8_t> record{0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0xFF,
                                           0xFF, 0x00, 0x02, 0xFF, 0xFD, 0x00, 0x04};
    std::vector<std::uint8_t> expected = prefix;
    expected.insert(expected.end(), record.begin(), record.end());

    EXPECT_EQ(encoded(snapshot), expected);
}

TEST(SnapshotPayload, EncodesEmptySnapshot) {
    EXPECT_EQ(encoded(snapshot_with(0)),
              (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04, 0x00, 0x00}));
}

TEST(SnapshotPayload, MaximumSnapshotFitsInOnePayload) {
    EXPECT_EQ(max_snapshot_entities, 84U);
    EXPECT_LE(encoded(snapshot_with(max_snapshot_entities)).size(), max_payload_size);
}

TEST(SnapshotPayload, EncoderRejectsTooManyEntities) {
    EXPECT_FALSE(encode_payload(snapshot_with(max_snapshot_entities + 1)).has_value());
}

TEST(ConnectAcceptPayload, EncodesFieldsInOrder) {
    const auto accept = accept_with({{.type_id = 0x0009, .name = "ab"}});

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                             0x00, 0x01, 0x00, 0x09, 0x02, 'a',  'b'};

    EXPECT_EQ(encode_payload(accept), expected);
}

TEST(ConnectAcceptPayload, EncoderRejectsEmptyName) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = ""}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsNameAboveMaximumLength) {
    const std::vector<TypeEntry> types{
        {.type_id = 1, .name = std::string(max_type_name_length + 1, 'a')}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsDuplicateTypeId) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "player"},
                                       {.type_id = 1, .name = "bydo"}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsDuplicateName) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "bydo"},
                                       {.type_id = 2, .name = "bydo"}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsTableAboveMaximumPayloadSize) {
    EXPECT_TRUE(encode_payload(accept_with(long_name_types(17))).has_value());
    EXPECT_FALSE(encode_payload(accept_with(long_name_types(18))).has_value());
}
