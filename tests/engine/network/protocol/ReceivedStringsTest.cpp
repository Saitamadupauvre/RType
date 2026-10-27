#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "PayloadSamples.hpp"
#include "protocol/ReceivedStrings.hpp"

namespace logger = engine::core::log;

namespace {

using engine::network::ConnectAcceptPayload;
using engine::network::DestroyEvent;
using engine::network::DisconnectPayload;
using engine::network::EntityState;
using engine::network::EventPayload;
using engine::network::HeartbeatPayload;
using engine::network::Message;
using engine::network::SnapshotPayload;
using engine::network::SoundEvent;
using engine::network::SpawnEvent;
using engine::network::StringCategory;

constexpr std::uint16_t player_type = 1;
constexpr std::uint16_t enemy_type = 2;
constexpr std::uint16_t unknown_type = 99;

ConnectAcceptPayload sample_accept() {
    return ConnectAcceptPayload{
        .client_nonce = 1,
        .player_entity_id = 1,
        .strings = payload_samples::strings_of(
            {{.category = StringCategory::type, .id = player_type, .name = "player"},
             {.category = StringCategory::type, .id = enemy_type, .name = "enemy"},
             {.category = StringCategory::sound, .id = unknown_type, .name = "shot.wav"}})};
}

Message snapshot_of(std::uint16_t first_type, std::uint16_t second_type) {
    return SnapshotPayload{.server_tick = 1,
                           .entities = {EntityState{.entity_id = 1, .type_id = first_type},
                                        EntityState{.entity_id = 2, .type_id = second_type}}};
}

Message spawn_of(std::uint16_t type_id) {
    return EventPayload{SpawnEvent{.entity_id = 3, .type_id = type_id}};
}

class ReceivedStrings : public testing::Test {
protected:
    void expect_dropped_and_logged(const Message& message, const std::string& expected) {
        EXPECT_FALSE(table.admits(message));
        const auto records = sink->records();
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records.front().level, logger::Level::Warn);
        EXPECT_NE(records.front().message.find(expected), std::string::npos)
            << records.front().message;
    }

    engine::network::ReceivedStrings table;
    std::shared_ptr<logger::MemorySink> sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped{sink, logger::Level::Trace};
};

} // namespace

TEST_F(ReceivedStrings, StartsUnassigned) {
    EXPECT_FALSE(table.is_assigned());
    EXPECT_FALSE(table.is_known_type(player_type));
}

TEST_F(ReceivedStrings, KnowsTypesFromConnectAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.is_assigned());
    EXPECT_TRUE(table.is_known_type(player_type));
    EXPECT_TRUE(table.is_known_type(enemy_type));
    EXPECT_FALSE(table.is_known_type(unknown_type));
}

TEST_F(ReceivedStrings, KeepsTheStringTableFromConnectAccept) {
    EXPECT_TRUE(table.table().entries().empty());

    table.assign(sample_accept());

    EXPECT_EQ(table.table(), sample_accept().strings);
}

TEST_F(ReceivedStrings, AnIdOfAnotherCategoryIsNotAType) {
    table.assign(sample_accept());

    EXPECT_FALSE(table.is_known_type(unknown_type));
}

TEST_F(ReceivedStrings, AssignReplacesPreviousTable) {
    table.assign(sample_accept());

    table.assign(ConnectAcceptPayload{
        .strings = payload_samples::strings_of(
            {{.category = StringCategory::type, .id = unknown_type, .name = "boss"}})});

    EXPECT_TRUE(table.is_known_type(unknown_type));
    EXPECT_FALSE(table.is_known_type(player_type));
    ASSERT_EQ(table.table().entries().size(), 1U);
    EXPECT_EQ(table.table().entries().front().name, "boss");
}

TEST_F(ReceivedStrings, AdmitsSnapshotWithKnownTypes) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(snapshot_of(player_type, enemy_type)));
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(ReceivedStrings, AdmitsEmptySnapshotAfterAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(SnapshotPayload{.server_tick = 1, .entities = {}}));
}

TEST_F(ReceivedStrings, DropsSnapshotWithOneUnknownType) {
    table.assign(sample_accept());

    expect_dropped_and_logged(snapshot_of(player_type, unknown_type), "unknown type id 99");
}

TEST_F(ReceivedStrings, AdmitsSpawnWithKnownType) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(spawn_of(enemy_type)));
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(ReceivedStrings, DropsSpawnWithUnknownType) {
    table.assign(sample_accept());

    expect_dropped_and_logged(spawn_of(unknown_type), "spawn event with unknown type id 99");
}

TEST_F(ReceivedStrings, AdmitsOtherEventsAfterAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(EventPayload{DestroyEvent{.entity_id = 3}}));
    EXPECT_TRUE(table.admits(EventPayload{SoundEvent{.source_entity_id = 3, .sound_id = 4}}));
}

TEST_F(ReceivedStrings, DropsSnapshotBeforeAccept) {
    expect_dropped_and_logged(snapshot_of(player_type, enemy_type), "before connect_accept");
}

TEST_F(ReceivedStrings, DropsSpawnBeforeAccept) {
    expect_dropped_and_logged(spawn_of(player_type), "before connect_accept");
}

TEST_F(ReceivedStrings, DropsOtherEventsBeforeAccept) {
    expect_dropped_and_logged(EventPayload{DestroyEvent{.entity_id = 3}}, "before connect_accept");
}

TEST_F(ReceivedStrings, AdmitsMessagesWithoutTypeIdsBeforeAccept) {
    EXPECT_TRUE(table.admits(HeartbeatPayload{}));
    EXPECT_TRUE(table.admits(DisconnectPayload{}));
    EXPECT_TRUE(table.admits(sample_accept()));
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(ReceivedStrings, AdmitsMusicEventsOnlyAfterAccept) {
    const Message music = EventPayload{engine::network::MusicEvent{.music_id = 2}};

    expect_dropped_and_logged(music, "before connect_accept");
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(music));
}
