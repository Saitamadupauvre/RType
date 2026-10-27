#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadSamples.hpp"
#include "encoder/PayloadEncoder.hpp"

using engine::core::AssetName;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::network::ConnectRequestPayload;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::encode_payload;
using engine::network::EntityState;
using engine::network::EventPayload;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::max_payload_size;
using engine::network::NamedEventState;
using engine::network::PlayerLeftEvent;
using engine::network::snapshot_prefix_size;
using engine::network::SnapshotPayload;
using engine::network::SoundEvent;
using engine::network::StringCategory;
using payload_samples::accept_with;
using payload_samples::encoded;
using payload_samples::long_name_strings;
using payload_samples::max_moving_entities;
using payload_samples::moving_entity_size;
using payload_samples::snapshot_with;
using payload_samples::strings_of;

namespace {

constexpr std::size_t first_field_offset = 6 + 7 + 2;

SnapshotPayload snapshot_of_field(FieldValue value) {
    return {.server_tick = 1,
            .entities = {EntityState{.entity_id = 1,
                                     .type_id = 1,
                                     .fields = {{.field_id = 1, .value = std::move(value)}}}}};
}

std::vector<std::uint8_t> encoded_field(FieldValue value) {
    const auto bytes = encoded(snapshot_of_field(std::move(value)));
    if (bytes.size() < first_field_offset) {
        return {};
    }
    return {bytes.begin() + first_field_offset, bytes.end()};
}

} // namespace

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

TEST(EventPayload, EncodesNamedEventArgumentsInOrder) {
    const EventPayload event = payload_samples::explosion_event();

    const std::vector<std::uint8_t> expected{0x07, 0x01, 0x02, 0x03, 0x00, 0x01, 0x04, 0xFF, 0xFE,
                                             0x00, 0x07, 0x00, 0x02, 0x00, 0x3F, 0xC0, 0x00, 0x00,
                                             0x00, 0x03, 0x03, 0x03, 'b',  'i',  'g'};

    EXPECT_EQ(encoded(event), expected);
}

TEST(EventPayload, EncodesNamedEventWithoutArguments) {
    const EventPayload event = NamedEventState{.name_id = 0x0304, .arguments = {}};

    EXPECT_EQ(encoded(event), (std::vector<std::uint8_t>{0x07, 0x03, 0x04, 0x00}));
}

TEST(EventPayload, EncoderRejectsNamedEventsBreakingTheLimits) {
    NamedEventState too_many{.name_id = 1, .arguments = {}};
    for (std::uint16_t key = 1; key <= engine::network::max_event_arguments + 1; ++key) {
        too_many.arguments.push_back({.key_id = key, .value = true});
    }
    const NamedEventState repeated_key{
        .name_id = 1, .arguments = {{.key_id = 1, .value = true}, {.key_id = 1, .value = false}}};
    const NamedEventState long_text{
        .name_id = 1,
        .arguments = {
            {.key_id = 1, .value = std::string(engine::network::max_event_text_length + 1, 'a')}}};
    const NamedEventState long_asset{
        .name_id = 1,
        .arguments = {{.key_id = 1,
                       .value = engine::core::AssetName{
                           std::string(engine::network::max_event_text_length + 1, 'a')}}}};
    const NamedEventState infinite{
        .name_id = 1,
        .arguments = {{.key_id = 1, .value = std::numeric_limits<float>::infinity()}}};
    const NamedEventState list{.name_id = 1,
                               .arguments = {{.key_id = 1, .value = engine::core::FieldList{}}}};

    for (const auto& event : {too_many, repeated_key, long_text, long_asset, infinite, list}) {
        EXPECT_FALSE(encode_payload(EventPayload{event}).has_value());
    }
}

TEST(EventPayload, EncodesTheLongestTextAnEventAllows) {
    const NamedEventState event{
        .name_id = 1,
        .arguments = {
            {.key_id = 1, .value = std::string(engine::network::max_event_text_length, 'a')}}};

    EXPECT_TRUE(encode_payload(EventPayload{event}).has_value());
}

TEST(EventPayload, EncodesSoundFieldsInOrder) {
    const EventPayload event = SoundEvent{.source_entity_id = 0x0102'0304, .sound_id = 0x0506};

    EXPECT_EQ(encoded(event),
              (std::vector<std::uint8_t>{0x04, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}));
}

TEST(EventPayload, EncodesMusicFieldsInOrder) {
    const EventPayload event = engine::network::MusicEvent{.music_id = 0x0506};

    EXPECT_EQ(encoded(event), (std::vector<std::uint8_t>{0x06, 0x05, 0x06}));
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
            .fields = {{.field_id = 0x0001, .value = Vec2{-1.F, 2.F}},
                       {.field_id = 0x0002,
                        .value = Rgba{.r = 0x0C, .g = 0x0D, .b = 0x0E, .a = 0x0F}}},
        }},
    };

    const std::vector<std::uint8_t> prefix{0x01, 0x02, 0x03, 0x04, 0x00, 0x01};
    const std::vector<std::uint8_t> record{0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x02,
                                           0x00, 0x01, 0x04, 0xFF, 0xFF, 0x00, 0x02,
                                           0x00, 0x02, 0x05, 0x0C, 0x0D, 0x0E, 0x0F};
    std::vector<std::uint8_t> expected = prefix;
    expected.insert(expected.end(), record.begin(), record.end());

    EXPECT_EQ(encoded(snapshot), expected);
}

TEST(SnapshotPayload, EncodesEntityWithoutFields) {
    const SnapshotPayload snapshot{
        .server_tick = 1,
        .entities = {EntityState{.entity_id = 2, .type_id = 3, .fields = {}}},
    };

    EXPECT_EQ(encoded(snapshot),
              (std::vector<std::uint8_t>{0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
                                         0x00, 0x03, 0x00}));
}

TEST(SnapshotPayload, EncodesFloatAsItsBits) {
    EXPECT_EQ(encoded_field(1.5F), (std::vector<std::uint8_t>{0x00, 0x3F, 0xC0, 0x00, 0x00}));
}

TEST(SnapshotPayload, EncodesIntAsThirtyTwoBits) {
    EXPECT_EQ(encoded_field(std::int64_t{-2}),
              (std::vector<std::uint8_t>{0x01, 0xFF, 0xFF, 0xFF, 0xFE}));
}

TEST(SnapshotPayload, ClampsIntToThirtyTwoBits) {
    EXPECT_EQ(encoded_field(std::int64_t{1'099'511'627'776}),
              (std::vector<std::uint8_t>{0x01, 0x7F, 0xFF, 0xFF, 0xFF}));
    EXPECT_EQ(encoded_field(std::int64_t{-1'099'511'627'776}),
              (std::vector<std::uint8_t>{0x01, 0x80, 0x00, 0x00, 0x00}));
}

TEST(SnapshotPayload, EncodesBoolAsOneByte) {
    EXPECT_EQ(encoded_field(true), (std::vector<std::uint8_t>{0x02, 0x01}));
    EXPECT_EQ(encoded_field(false), (std::vector<std::uint8_t>{0x02, 0x00}));
}

TEST(SnapshotPayload, EncodesStringAndAssetWithTheirLength) {
    EXPECT_EQ(encoded_field(std::string("ab")), (std::vector<std::uint8_t>{0x03, 0x02, 'a', 'b'}));
    EXPECT_EQ(encoded_field(AssetName{"c"}), (std::vector<std::uint8_t>{0x06, 0x01, 'c'}));
}

TEST(SnapshotPayload, RoundsVec2ToWholeCoordinates) {
    EXPECT_EQ(encoded_field(Vec2{1.6F, -40'000.F}),
              (std::vector<std::uint8_t>{0x04, 0x00, 0x02, 0x80, 0x00}));
}

TEST(SnapshotPayload, EncodesEmptySnapshot) {
    EXPECT_EQ(encoded(snapshot_with(0)),
              (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04, 0x00, 0x00}));
}

TEST(SnapshotPayload, MaximumSnapshotFitsInOnePayload) {
    EXPECT_EQ(encoded(snapshot_with(max_moving_entities)).size(),
              snapshot_prefix_size + max_moving_entities * moving_entity_size);
    EXPECT_LE(encoded(snapshot_with(max_moving_entities)).size(), max_payload_size);
}

TEST(SnapshotPayload, EncoderRejectsTooManyBytes) {
    EXPECT_FALSE(encode_payload(snapshot_with(max_moving_entities + 1)).has_value());
}

TEST(SnapshotPayload, EncoderRejectsListField) {
    EXPECT_FALSE(encode_payload(snapshot_of_field(FieldList{})).has_value());
}

TEST(SnapshotPayload, EncoderRejectsTooLongText) {
    EXPECT_TRUE(encode_payload(snapshot_of_field(std::string(255, 'a'))).has_value());
    EXPECT_FALSE(encode_payload(snapshot_of_field(std::string(256, 'a'))).has_value());
    EXPECT_FALSE(encode_payload(snapshot_of_field(AssetName{std::string(256, 'a')})).has_value());
}

TEST(SnapshotPayload, EncoderRejectsNonFiniteFloat) {
    EXPECT_FALSE(
        encode_payload(snapshot_of_field(std::numeric_limits<float>::quiet_NaN())).has_value());
    EXPECT_FALSE(
        encode_payload(snapshot_of_field(std::numeric_limits<float>::infinity())).has_value());
}

TEST(SnapshotPayload, EncoderRejectsTheSameFieldTwice) {
    SnapshotPayload snapshot = snapshot_of_field(true);
    snapshot.entities.front().fields.push_back(snapshot.entities.front().fields.front());

    EXPECT_FALSE(encode_payload(snapshot).has_value());
}

TEST(SnapshotPayload, EncoderRejectsMoreThan255Fields) {
    SnapshotPayload snapshot = snapshot_of_field(true);
    auto& fields = snapshot.entities.front().fields;
    for (std::uint16_t id = 2; id <= 256; ++id) {
        fields.push_back({.field_id = id, .value = true});
    }

    EXPECT_FALSE(encode_payload(snapshot).has_value());
    fields.pop_back();
    EXPECT_TRUE(encode_payload(snapshot).has_value());
}

TEST(ConnectAcceptPayload, EncodesFieldsInOrder) {
    const auto accept =
        accept_with(strings_of({{.category = StringCategory::sound, .id = 0x0009, .name = "ab"}}));

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x00,
                                             0x01, 0x01, 0x00, 0x09, 0x02, 'a',  'b',  0xFF, 0xFF};

    EXPECT_EQ(encode_payload(accept), expected);
}

TEST(ConnectAcceptPayload, EncodesEntriesOfEveryCategoryInOneTable) {
    auto accept =
        accept_with(strings_of({{.category = StringCategory::type, .id = 0x0001, .name = "t"},
                                {.category = StringCategory::music, .id = 0x0002, .name = "m"}}));
    accept.current_music = 0x0002;

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                             0x00, 0x02, 0x00, 0x00, 0x01, 0x01, 't',  0x02,
                                             0x00, 0x02, 0x01, 'm',  0x00, 0x02};

    EXPECT_EQ(encode_payload(accept), expected);
}

TEST(ConnectAcceptPayload, EncoderRejectsACurrentMusicMissingFromTheTable) {
    auto accept =
        accept_with(strings_of({{.category = StringCategory::music, .id = 1, .name = "a.ogg"},
                                {.category = StringCategory::sound, .id = 2, .name = "b.wav"}}));
    accept.current_music = 2;

    EXPECT_FALSE(encode_payload(accept).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsTableAboveMaximumPayloadSize) {
    EXPECT_TRUE(encode_payload(accept_with(long_name_strings(17))).has_value());
    EXPECT_FALSE(encode_payload(accept_with(long_name_strings(18))).has_value());
}
