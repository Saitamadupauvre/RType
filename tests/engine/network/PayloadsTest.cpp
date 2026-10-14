#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Payloads.hpp"

using engine::network::ConnectAcceptPayload;
using engine::network::ConnectRequestPayload;
using engine::network::decode_connect_accept;
using engine::network::decode_connect_request;
using engine::network::decode_disconnect;
using engine::network::decode_event;
using engine::network::decode_heartbeat;
using engine::network::decode_input;
using engine::network::decode_snapshot;
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
using engine::network::SnapshotPayload;
using engine::network::SoundEvent;
using engine::network::SpawnEvent;
using engine::network::TypeEntry;

namespace {

template <typename Payload> std::vector<std::uint8_t> encoded(const Payload& payload) {
    auto bytes = encode_payload(payload);
    EXPECT_TRUE(bytes.has_value());
    return bytes.value_or(std::vector<std::uint8_t>{});
}

} // namespace

TEST(InputPayload, EncodesFieldsInOrder) {
    const InputPayload input{.action_flags = 0x01, .move_x = -1, .move_y = 1};

    EXPECT_EQ(encode_payload(input), (std::vector<std::uint8_t>{0x01, 0xFF, 0x01}));
}

TEST(InputPayload, DecodesValidPayload) {
    const std::array<std::uint8_t, 3> payload{0x01, 0xFF, 0x01};

    const InputPayload expected{.action_flags = 0x01, .move_x = -1, .move_y = 1};

    EXPECT_EQ(decode_input(payload), expected);
}

TEST(InputPayload, RoundTripsEveryValidCombination) {
    constexpr std::array<std::uint8_t, 2> flag_values{0x00, 0x01};
    constexpr std::array<std::int8_t, 3> axis_values{-1, 0, 1};

    for (const auto flags : flag_values) {
        for (const auto move_x : axis_values) {
            for (const auto move_y : axis_values) {
                const InputPayload input{.action_flags = flags, .move_x = move_x, .move_y = move_y};

                EXPECT_EQ(decode_input(encoded(input)), input);
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
    const ConnectRequestPayload request{.client_nonce = 0x0102'0304};

    EXPECT_EQ(encode_payload(request), (std::vector<std::uint8_t>{0x01, 0x02, 0x03, 0x04}));
}

TEST(ConnectRequestPayload, RoundTripsBoundaryNonces) {
    constexpr std::array<std::uint32_t, 3> nonces{0x0000'0000, 0x8000'0001, 0xFFFF'FFFF};

    for (const auto nonce : nonces) {
        const ConnectRequestPayload request{.client_nonce = nonce};

        EXPECT_EQ(decode_connect_request(encoded(request)), request);
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
    EXPECT_EQ(encode_payload(HeartbeatPayload{}), std::vector<std::uint8_t>{});
}

TEST(HeartbeatPayload, RoundTrips) {
    EXPECT_EQ(decode_heartbeat(encoded(HeartbeatPayload{})), HeartbeatPayload{});
}

TEST(HeartbeatPayload, DecoderRejectsNonEmptyPayload) {
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x00}).has_value());
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x01, 0x02}).has_value());
}

TEST(DisconnectPayload, EncodesReasonAsOneByte) {
    const DisconnectPayload disconnect{.reason = DisconnectReason::server_shutdown};

    EXPECT_EQ(encode_payload(disconnect), (std::vector<std::uint8_t>{0x02}));
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

        EXPECT_EQ(decode_disconnect(encoded(disconnect)), disconnect);
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

namespace {

std::array<EventPayload, 4> sample_events() {
    return {
        SpawnEvent{.entity_id = 0x0102'0304, .type_id = 0x0506, .x = -2, .y = 0x0708},
        DestroyEvent{.entity_id = 0x0102'0304},
        FireEvent{.source_entity_id = 0x0102'0304, .projectile_entity_id = 0x0506'0708},
        SoundEvent{.source_entity_id = 0x0102'0304, .sound_id = 0x0506},
    };
}

} // namespace

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

TEST(EventPayload, RoundTripsEveryEvent) {
    for (const auto& event : sample_events()) {
        EXPECT_EQ(decode_event(encoded(event)), event) << "index " << event.index();
    }
}

TEST(EventPayload, DecoderRejectsEmptyPayload) {
    EXPECT_FALSE(decode_event(std::vector<std::uint8_t>{}).has_value());
}

TEST(EventPayload, DecoderRejectsUnknownEventTypes) {
    for (unsigned int type = 0; type <= 255; ++type) {
        if (type >= 1 && type <= 4) {
            continue;
        }
        std::vector<std::uint8_t> payload(11, 0x00);
        payload.front() = static_cast<std::uint8_t>(type);

        EXPECT_FALSE(decode_event(payload).has_value()) << "type " << type;
    }
}

TEST(EventPayload, DecoderRejectsTruncatedEvents) {
    for (const auto& event : sample_events()) {
        const auto bytes = encoded(event);

        for (std::size_t size = 1; size < bytes.size(); ++size) {
            const std::span<const std::uint8_t> truncated(bytes.data(), size);

            EXPECT_FALSE(decode_event(truncated).has_value())
                << "index " << event.index() << " size " << size;
        }
    }
}

TEST(EventPayload, DecoderRejectsTrailingBytes) {
    for (const auto& event : sample_events()) {
        auto bytes = encoded(event);
        bytes.push_back(0x00);

        EXPECT_FALSE(decode_event(bytes).has_value()) << "index " << event.index();
    }
}

namespace {

SnapshotPayload snapshot_with(std::size_t entity_count) {
    SnapshotPayload snapshot{.server_tick = 0x0102'0304, .entities = {}};
    for (std::size_t index = 0; index < entity_count; ++index) {
        const auto value = static_cast<std::int16_t>(index);
        snapshot.entities.push_back(EntityState{
            .entity_id = static_cast<std::uint32_t>(index + 1),
            .type_id = static_cast<std::uint16_t>(index % 3),
            .x = value,
            .y = static_cast<std::int16_t>(-value),
            .vx = static_cast<std::int16_t>(value * 2),
            .vy = static_cast<std::int16_t>(-value * 2),
        });
    }
    return snapshot;
}

} // namespace

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

TEST(SnapshotPayload, RoundTripsZeroOneAndMaximumEntities) {
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, max_snapshot_entities}) {
        const auto snapshot = snapshot_with(count);

        EXPECT_EQ(decode_snapshot(encoded(snapshot)), snapshot) << "count " << count;
    }
}

TEST(SnapshotPayload, MaximumSnapshotFitsInOnePayload) {
    EXPECT_EQ(max_snapshot_entities, 84U);
    EXPECT_LE(encoded(snapshot_with(max_snapshot_entities)).size(), max_payload_size);
}

TEST(SnapshotPayload, EncoderRejectsTooManyEntities) {
    EXPECT_FALSE(encode_payload(snapshot_with(max_snapshot_entities + 1)).has_value());
}

TEST(SnapshotPayload, DecoderRejectsTruncatedPrefix) {
    const auto bytes = encoded(snapshot_with(0));

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_snapshot(truncated).has_value()) << "size " << size;
    }
}

TEST(SnapshotPayload, DecoderRejectsTruncatedRecord) {
    const auto bytes = encoded(snapshot_with(2));
    const std::span<const std::uint8_t> truncated(bytes.data(), bytes.size() - 1);

    EXPECT_FALSE(decode_snapshot(truncated).has_value());
}

TEST(SnapshotPayload, DecoderRejectsTrailingBytes) {
    auto bytes = encoded(snapshot_with(2));
    bytes.push_back(0x00);

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
}

TEST(SnapshotPayload, DecoderRejectsCountLargerThanRecords) {
    auto bytes = encoded(snapshot_with(1));
    bytes[5] = 0x02;

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
}

TEST(SnapshotPayload, DecoderRejectsCountSmallerThanRecords) {
    auto bytes = encoded(snapshot_with(2));
    bytes[5] = 0x01;

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
}

TEST(SnapshotPayload, DecoderRejectsHugeCountInShortPayload) {
    const std::array<std::uint8_t, 13> payload{0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(decode_snapshot(payload).has_value());
}

TEST(SnapshotPayload, DecoderRejectsMoreThanMaximumEntities) {
    const std::size_t count = max_snapshot_entities + 1;
    std::vector<std::uint8_t> bytes{0x00, 0x00, 0x00, 0x01, 0x00, static_cast<std::uint8_t>(count)};
    bytes.resize(bytes.size() + (count * engine::network::entity_record_size), 0x00);

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
}

namespace {

ConnectAcceptPayload accept_with(std::vector<TypeEntry> types) {
    return {
        .client_nonce = 0x0102'0304,
        .player_entity_id = 0x0506'0708,
        .types = std::move(types),
    };
}

std::vector<std::uint8_t> raw_accept(std::uint16_t count, const std::vector<TypeEntry>& types) {
    std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    bytes.push_back(static_cast<std::uint8_t>(count >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(count));
    for (const auto& entry : types) {
        bytes.push_back(static_cast<std::uint8_t>(entry.type_id >> 8U));
        bytes.push_back(static_cast<std::uint8_t>(entry.type_id));
        bytes.push_back(static_cast<std::uint8_t>(entry.name.size()));
        bytes.insert(bytes.end(), entry.name.begin(), entry.name.end());
    }
    return bytes;
}

std::vector<std::uint8_t> raw_accept(const std::vector<TypeEntry>& types) {
    return raw_accept(static_cast<std::uint16_t>(types.size()), types);
}

std::vector<TypeEntry> long_name_types(std::size_t count) {
    std::vector<TypeEntry> types;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name(max_type_name_length, 'a');
        name.back() = static_cast<char>('A' + index);
        types.push_back({.type_id = static_cast<std::uint16_t>(index), .name = std::move(name)});
    }
    return types;
}

const std::vector<TypeEntry> sample_types{{.type_id = 1, .name = "player"},
                                          {.type_id = 2, .name = "bydo"},
                                          {.type_id = 7, .name = "player_missile"}};

} // namespace

TEST(ConnectAcceptPayload, EncodesFieldsInOrder) {
    const auto accept = accept_with({{.type_id = 0x0009, .name = "ab"}});

    const std::vector<std::uint8_t> expected{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                             0x00, 0x01, 0x00, 0x09, 0x02, 'a',  'b'};

    EXPECT_EQ(encode_payload(accept), expected);
}

TEST(ConnectAcceptPayload, RoundTripsTypeTable) {
    const auto accept = accept_with(sample_types);

    EXPECT_EQ(decode_connect_accept(encoded(accept)), accept);
}

TEST(ConnectAcceptPayload, RoundTripsEmptyTypeTable) {
    const auto accept = accept_with({});

    const auto bytes = encoded(accept);

    EXPECT_EQ(bytes.size(), engine::network::connect_accept_prefix_size);
    EXPECT_EQ(decode_connect_accept(bytes), accept);
}

TEST(ConnectAcceptPayload, AcceptsMaximumNameLength) {
    const auto accept = accept_with(long_name_types(1));

    EXPECT_EQ(decode_connect_accept(encoded(accept)), accept);
}

TEST(ConnectAcceptPayload, RejectsEmptyName) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = ""}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, RejectsNameAboveMaximumLength) {
    const std::vector<TypeEntry> types{
        {.type_id = 1, .name = std::string(max_type_name_length + 1, 'a')}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, RejectsDuplicateTypeId) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "player"},
                                       {.type_id = 1, .name = "bydo"}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, RejectsDuplicateName) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "bydo"},
                                       {.type_id = 2, .name = "bydo"}};

    EXPECT_FALSE(encode_payload(accept_with(types)).has_value());
    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, EncoderRejectsTableAboveMaximumPayloadSize) {
    EXPECT_TRUE(encode_payload(accept_with(long_name_types(17))).has_value());
    EXPECT_FALSE(encode_payload(accept_with(long_name_types(18))).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTruncatedPrefix) {
    const auto bytes = raw_accept({});

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_connect_accept(truncated).has_value()) << "size " << size;
    }
}

TEST(ConnectAcceptPayload, DecoderRejectsNameRunningPastTheEnd) {
    auto bytes = raw_accept({{.type_id = 1, .name = "bydo"}});
    bytes.pop_back();

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsCountLargerThanEntries) {
    const auto bytes = raw_accept(2, {{.type_id = 1, .name = "bydo"}});

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsHugeCountInShortPayload) {
    EXPECT_FALSE(decode_connect_accept(raw_accept(0xFFFF, sample_types)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTrailingBytes) {
    auto bytes = raw_accept(sample_types);
    bytes.push_back(0x00);

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}
