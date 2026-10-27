#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadSamples.hpp"
#include "decoder/PayloadDecoder.hpp"

using engine::core::AssetName;
using engine::core::Rgba;
using engine::core::Vec2;
using engine::network::ConnectRequestPayload;
using engine::network::decode_connect_accept;
using engine::network::decode_connect_request;
using engine::network::decode_disconnect;
using engine::network::decode_event;
using engine::network::decode_heartbeat;
using engine::network::decode_input;
using engine::network::decode_snapshot;
using engine::network::DisconnectPayload;
using engine::network::DisconnectReason;
using engine::network::entity_header_size;
using engine::network::EntityState;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::max_snapshot_entity_bytes;
using engine::network::max_string_name_length;
using engine::network::SnapshotPayload;
using engine::network::StringCategory;
using payload_samples::accept_with;
using payload_samples::encoded;
using payload_samples::long_name_strings;
using payload_samples::max_moving_entities;
using payload_samples::sample_events;
using payload_samples::snapshot_with;
using payload_samples::strings_of;

namespace {

constexpr std::array<std::uint8_t, 2> no_current_music{0xFF, 0xFF};
constexpr std::size_t snapshot_header_size = 6;

std::vector<std::uint8_t> snapshot_bytes_of_field(std::uint8_t kind,
                                                  std::vector<std::uint8_t> value) {
    std::vector<std::uint8_t> bytes{0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
                                    0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x05, kind};
    bytes.insert(bytes.end(), value.begin(), value.end());
    return bytes;
}

struct RawEntry {
    std::uint8_t category;
    std::uint16_t id;
    std::string name;
};

std::vector<std::uint8_t> raw_accept(std::uint16_t count, const std::vector<RawEntry>& entries) {
    std::vector<std::uint8_t> bytes{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    bytes.push_back(static_cast<std::uint8_t>(count >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(count));
    for (const auto& entry : entries) {
        bytes.push_back(entry.category);
        bytes.push_back(static_cast<std::uint8_t>(entry.id >> 8U));
        bytes.push_back(static_cast<std::uint8_t>(entry.id));
        bytes.push_back(static_cast<std::uint8_t>(entry.name.size()));
        bytes.insert(bytes.end(), entry.name.begin(), entry.name.end());
    }
    bytes.insert(bytes.end(), no_current_music.begin(), no_current_music.end());
    return bytes;
}

std::vector<std::uint8_t> raw_accept(const std::vector<RawEntry>& entries) {
    return raw_accept(static_cast<std::uint16_t>(entries.size()), entries);
}

engine::network::StringTable sample_strings() {
    return strings_of({{.category = StringCategory::type, .id = 1, .name = "player"},
                       {.category = StringCategory::type, .id = 2, .name = "bydo"},
                       {.category = StringCategory::sound, .id = 1, .name = "shot.wav"},
                       {.category = StringCategory::music, .id = 7, .name = "stage1.ogg"},
                       {.category = StringCategory::field, .id = 1, .name = "Position.pos"}});
}

} // namespace

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

TEST(HeartbeatPayload, RoundTrips) {
    EXPECT_EQ(decode_heartbeat(encoded(HeartbeatPayload{})), HeartbeatPayload{});
}

TEST(HeartbeatPayload, DecoderRejectsNonEmptyPayload) {
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x00}).has_value());
    EXPECT_FALSE(decode_heartbeat(std::vector<std::uint8_t>{0x01, 0x02}).has_value());
}

TEST(DisconnectPayload, RoundTripsEveryDefinedReason) {
    constexpr std::array<DisconnectReason, 5> reasons{
        DisconnectReason::normal,          DisconnectReason::timeout,
        DisconnectReason::server_shutdown, DisconnectReason::protocol_error,
        DisconnectReason::server_full,
    };

    for (const auto reason : reasons) {
        const DisconnectPayload disconnect{.reason = reason};

        EXPECT_EQ(decode_disconnect(encoded(disconnect)), disconnect);
    }
}

TEST(DisconnectPayload, DecoderRejectsUndefinedReasons) {
    for (unsigned int value = 5; value <= 255; ++value) {
        const std::array<std::uint8_t, 1> payload{static_cast<std::uint8_t>(value)};

        EXPECT_FALSE(decode_disconnect(payload).has_value()) << "reason " << value;
    }
}

TEST(DisconnectPayload, DecoderRejectsWrongSize) {
    EXPECT_FALSE(decode_disconnect(std::vector<std::uint8_t>{}).has_value());
    EXPECT_FALSE(decode_disconnect(std::vector<std::uint8_t>{0x00, 0x00}).has_value());
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
        if (type >= 1 && type <= 6) {
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

TEST(SnapshotPayload, RoundTripsEveryFieldKind) {
    const SnapshotPayload snapshot{
        .server_tick = 9,
        .entities = {EntityState{
            .entity_id = 1,
            .type_id = 1,
            .fields = {{.field_id = 1, .value = -1.25F},
                       {.field_id = 2, .value = std::int64_t{-70'000}},
                       {.field_id = 3, .value = true},
                       {.field_id = 4, .value = std::string("hello")},
                       {.field_id = 5, .value = Vec2{-3.F, 4.F}},
                       {.field_id = 6, .value = Rgba{.r = 1, .g = 2, .b = 3, .a = 4}},
                       {.field_id = 7, .value = AssetName{"sprites/ship.png"}},
                       {.field_id = 8, .value = std::string()}}}},
    };

    EXPECT_EQ(decode_snapshot(encoded(snapshot)), snapshot);
}

TEST(SnapshotPayload, RoundTripsZeroOneAndMaximumEntities) {
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, max_moving_entities}) {
        const auto snapshot = snapshot_with(count);

        EXPECT_EQ(decode_snapshot(encoded(snapshot)), snapshot) << "count " << count;
    }
}

TEST(SnapshotPayload, DecoderRejectsTruncatedPrefix) {
    const auto bytes = encoded(snapshot_with(0));

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_snapshot(truncated).has_value()) << "size " << size;
    }
}

TEST(SnapshotPayload, DecoderRejectsEveryTruncatedEntity) {
    const SnapshotPayload snapshot{
        .server_tick = 9,
        .entities = {EntityState{.entity_id = 1,
                                 .type_id = 1,
                                 .fields = {{.field_id = 1, .value = Vec2{1.F, 2.F}},
                                            {.field_id = 2, .value = std::string("ab")}}}},
    };
    const auto bytes = encoded(snapshot);

    for (std::size_t size = 6; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_snapshot(truncated).has_value()) << "size " << size;
    }
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

TEST(SnapshotPayload, DecoderRejectsFieldCountLargerThanFields) {
    auto bytes = encoded(snapshot_with(1));
    bytes[snapshot_header_size + 6] = 0x03;

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
}

TEST(SnapshotPayload, DecoderRejectsHugeCountInShortPayload) {
    const std::array<std::uint8_t, 13> payload{0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(decode_snapshot(payload).has_value());
}

TEST(SnapshotPayload, DecoderRejectsMoreBytesThanOneSnapshotHolds) {
    const std::size_t fitting = max_snapshot_entity_bytes / entity_header_size;
    for (const std::size_t count : {fitting, fitting + 1}) {
        std::vector<std::uint8_t> bytes{0x00, 0x00, 0x00,
                                        0x01, 0x00, static_cast<std::uint8_t>(count)};
        bytes.resize(bytes.size() + (count * entity_header_size), 0x00);

        EXPECT_EQ(decode_snapshot(bytes).has_value(), count == fitting) << "count " << count;
    }
}

TEST(SnapshotPayload, DecoderRejectsUnknownFieldKind) {
    for (const std::uint8_t kind : {std::uint8_t{0x07}, std::uint8_t{0x08}, std::uint8_t{0xFF}}) {
        EXPECT_FALSE(
            decode_snapshot(snapshot_bytes_of_field(kind, {0x00, 0x00, 0x00, 0x00})).has_value())
            << "kind " << int{kind};
    }
}

TEST(SnapshotPayload, DecoderRejectsBoolOtherThanZeroOrOne) {
    EXPECT_TRUE(decode_snapshot(snapshot_bytes_of_field(0x02, {0x01})).has_value());
    EXPECT_FALSE(decode_snapshot(snapshot_bytes_of_field(0x02, {0x02})).has_value());
}

TEST(SnapshotPayload, DecoderRejectsNonFiniteFloat) {
    EXPECT_TRUE(
        decode_snapshot(snapshot_bytes_of_field(0x00, {0x3F, 0x80, 0x00, 0x00})).has_value());
    EXPECT_FALSE(
        decode_snapshot(snapshot_bytes_of_field(0x00, {0x7F, 0xC0, 0x00, 0x00})).has_value());
    EXPECT_FALSE(
        decode_snapshot(snapshot_bytes_of_field(0x00, {0x7F, 0x80, 0x00, 0x00})).has_value());
}

TEST(SnapshotPayload, DecoderRejectsTextLongerThanItsBytes) {
    EXPECT_FALSE(decode_snapshot(snapshot_bytes_of_field(0x03, {0x03, 'a', 'b'})).has_value());
    EXPECT_FALSE(decode_snapshot(snapshot_bytes_of_field(0x06, {0x03, 'a', 'b'})).has_value());
}

TEST(SnapshotPayload, DecoderRejectsTheSameFieldTwice) {
    std::vector<std::uint8_t> bytes{0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00,
                                    0x00, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00,
                                    0x05, 0x02, 0x01, 0x00, 0x05, 0x02, 0x00};

    EXPECT_FALSE(decode_snapshot(bytes).has_value());
    bytes[18] = 0x06;
    EXPECT_TRUE(decode_snapshot(bytes).has_value());
}

TEST(ConnectAcceptPayload, RoundTripsStringTable) {
    auto accept = accept_with(sample_strings());
    accept.current_music = 7;

    EXPECT_EQ(decode_connect_accept(encoded(accept)), accept);
}

TEST(ConnectAcceptPayload, RoundTripsEmptyStringTable) {
    const auto accept = accept_with({});

    const auto bytes = encoded(accept);

    EXPECT_EQ(bytes.size(), engine::network::connect_accept_prefix_size);
    EXPECT_EQ(decode_connect_accept(bytes), accept);
}

TEST(ConnectAcceptPayload, AcceptsMaximumNameLength) {
    const auto accept = accept_with(long_name_strings(1));

    EXPECT_EQ(decode_connect_accept(encoded(accept)), accept);
}

TEST(ConnectAcceptPayload, AcceptsTheSameIdAndNameInTwoCategories) {
    const auto decoded =
        decode_connect_accept(raw_accept({{.category = 0, .id = 1, .name = "boss"},
                                          {.category = 1, .id = 1, .name = "boss"},
                                          {.category = 2, .id = 1, .name = "boss"}}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->strings.id_of(StringCategory::music, "boss"), 1U);
}

TEST(ConnectAcceptPayload, DecoderRejectsUnknownCategory) {
    EXPECT_TRUE(
        decode_connect_accept(raw_accept({{.category = 5, .id = 1, .name = "a"}})).has_value());
    EXPECT_FALSE(
        decode_connect_accept(raw_accept({{.category = 6, .id = 1, .name = "a"}})).has_value());
    EXPECT_FALSE(
        decode_connect_accept(raw_accept({{.category = 0xFF, .id = 1, .name = "a"}})).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsEmptyName) {
    EXPECT_FALSE(
        decode_connect_accept(raw_accept({{.category = 0, .id = 1, .name = ""}})).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsNameAboveMaximumLength) {
    const std::vector<RawEntry> entries{
        {.category = 0, .id = 1, .name = std::string(max_string_name_length + 1, 'a')}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(entries)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsDuplicateIdInACategory) {
    const std::vector<RawEntry> entries{{.category = 1, .id = 1, .name = "shot.wav"},
                                        {.category = 1, .id = 1, .name = "boom.wav"}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(entries)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsDuplicateNameInACategory) {
    const std::vector<RawEntry> entries{{.category = 0, .id = 1, .name = "bydo"},
                                        {.category = 0, .id = 2, .name = "bydo"}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(entries)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTheReservedId) {
    EXPECT_FALSE(decode_connect_accept(raw_accept({{.category = 2, .id = 0xFFFF, .name = "a.ogg"}}))
                     .has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTruncatedPayloads) {
    auto accept = accept_with(sample_strings());
    accept.current_music = 7;
    const auto bytes = encoded(accept);

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_connect_accept(truncated).has_value()) << "size " << size;
    }
}

TEST(ConnectAcceptPayload, DecoderRejectsNameRunningPastTheEnd) {
    auto bytes = raw_accept({{.category = 0, .id = 1, .name = "bydo"}});
    bytes.resize(bytes.size() - no_current_music.size() - 1);

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsCountLargerThanEntries) {
    const auto bytes = raw_accept(2, {{.category = 0, .id = 1, .name = "bydo"}});

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsHugeCountInShortPayload) {
    EXPECT_FALSE(
        decode_connect_accept(raw_accept(0xFFFF, {{.category = 0, .id = 1, .name = "bydo"}}))
            .has_value());
}

TEST(ConnectAcceptPayload, DecodesTheRawLayoutOfAnEmptyAccept) {
    const auto decoded = decode_connect_accept(raw_accept({}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded, accept_with({}));
}

TEST(ConnectAcceptPayload, DecoderRejectsACurrentMusicMissingFromTheTable) {
    auto bytes = raw_accept({{.category = 1, .id = 1, .name = "shot.wav"}});
    bytes.back() = 0x01;
    bytes[bytes.size() - 2] = 0x00;

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTrailingBytes) {
    auto bytes = raw_accept({{.category = 0, .id = 1, .name = "bydo"}});
    bytes.push_back(0x00);

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}
