#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "PayloadSamples.hpp"
#include "decoder/PayloadDecoder.hpp"

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
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::max_snapshot_entities;
using engine::network::max_type_name_length;
using engine::network::TypeEntry;
using payload_samples::accept_with;
using payload_samples::encoded;
using payload_samples::long_name_types;
using payload_samples::sample_events;
using payload_samples::snapshot_with;

namespace {

constexpr std::array<std::uint8_t, 6> empty_audio_tables{0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF};

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
    bytes.insert(bytes.end(), empty_audio_tables.begin(), empty_audio_tables.end());
    return bytes;
}

std::vector<std::uint8_t> raw_accept(const std::vector<TypeEntry>& types) {
    return raw_accept(static_cast<std::uint16_t>(types.size()), types);
}

std::vector<TypeEntry> sample_types() {
    return {{.type_id = 1, .name = "player"},
            {.type_id = 2, .name = "bydo"},
            {.type_id = 7, .name = "player_missile"}};
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

TEST(SnapshotPayload, RoundTripsOwnerAndColor) {
    const engine::network::SnapshotPayload snapshot{
        .server_tick = 9,
        .entities = {engine::network::EntityState{
            .entity_id = 1, .type_id = 1, .owner = 3, .r = 1, .g = 2, .b = 3, .a = 4}},
    };

    EXPECT_EQ(decode_snapshot(encoded(snapshot)), snapshot);
}

TEST(SnapshotPayload, RoundTripsZeroOneAndMaximumEntities) {
    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, max_snapshot_entities}) {
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

TEST(ConnectAcceptPayload, RoundTripsTypeTable) {
    const auto accept = accept_with(sample_types());

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

TEST(ConnectAcceptPayload, DecoderRejectsEmptyName) {
    EXPECT_FALSE(decode_connect_accept(raw_accept({{.type_id = 1, .name = ""}})).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsNameAboveMaximumLength) {
    const std::vector<TypeEntry> types{
        {.type_id = 1, .name = std::string(max_type_name_length + 1, 'a')}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsDuplicateTypeId) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "player"},
                                       {.type_id = 1, .name = "bydo"}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsDuplicateName) {
    const std::vector<TypeEntry> types{{.type_id = 1, .name = "bydo"},
                                       {.type_id = 2, .name = "bydo"}};

    EXPECT_FALSE(decode_connect_accept(raw_accept(types)).has_value());
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
    bytes.resize(bytes.size() - empty_audio_tables.size() - 1);

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsCountLargerThanEntries) {
    const auto bytes = raw_accept(2, {{.type_id = 1, .name = "bydo"}});

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsHugeCountInShortPayload) {
    EXPECT_FALSE(decode_connect_accept(raw_accept(0xFFFF, sample_types())).has_value());
}

TEST(ConnectAcceptPayload, DecodesTheRawLayoutOfAnEmptyAccept) {
    const auto decoded = decode_connect_accept(raw_accept({}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded, accept_with({}));
}

TEST(ConnectAcceptPayload, RoundTripsSoundAndMusicTables) {
    auto accept = accept_with(sample_types());
    accept.sounds = {{.type_id = 1, .name = "shot.wav"}, {.type_id = 2, .name = "boom.wav"}};
    accept.musics = {{.type_id = 1, .name = "stage1.ogg"}};
    accept.current_music = 1;

    EXPECT_EQ(decode_connect_accept(encoded(accept)), accept);
}

TEST(ConnectAcceptPayload, DecoderRejectsMissingAudioTables) {
    auto bytes = raw_accept(sample_types());
    bytes.resize(bytes.size() - empty_audio_tables.size());

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTruncatedAudioTables) {
    auto accept = accept_with({});
    accept.sounds = {{.type_id = 1, .name = "shot.wav"}};
    accept.musics = {{.type_id = 1, .name = "stage1.ogg"}};
    const auto bytes = encoded(accept);

    for (std::size_t size = 0; size < bytes.size(); ++size) {
        const std::span<const std::uint8_t> truncated(bytes.data(), size);

        EXPECT_FALSE(decode_connect_accept(truncated).has_value()) << "size " << size;
    }
}

TEST(ConnectAcceptPayload, DecoderRejectsACurrentMusicMissingFromTheTable) {
    auto bytes = raw_accept({});
    bytes.back() = 0x01;
    bytes[bytes.size() - 2] = 0x00;

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}

TEST(ConnectAcceptPayload, DecoderRejectsTrailingBytes) {
    auto bytes = raw_accept(sample_types());
    bytes.push_back(0x00);

    EXPECT_FALSE(decode_connect_accept(bytes).has_value());
}
