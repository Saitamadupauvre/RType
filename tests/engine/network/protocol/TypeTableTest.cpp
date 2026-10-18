#include <cstdint>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "protocol/TypeTable.hpp"

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

constexpr std::uint16_t player_type = 1;
constexpr std::uint16_t enemy_type = 2;
constexpr std::uint16_t unknown_type = 99;

ConnectAcceptPayload sample_accept() {
    return ConnectAcceptPayload{.client_nonce = 1,
                                .player_entity_id = 1,
                                .types = {{.type_id = player_type, .name = "player"},
                                          {.type_id = enemy_type, .name = "enemy"}}};
}

Message snapshot_of(std::uint16_t first_type, std::uint16_t second_type) {
    return SnapshotPayload{.server_tick = 1,
                           .entities = {EntityState{.entity_id = 1, .type_id = first_type},
                                        EntityState{.entity_id = 2, .type_id = second_type}}};
}

Message spawn_of(std::uint16_t type_id) {
    return EventPayload{SpawnEvent{.entity_id = 3, .type_id = type_id}};
}

class TypeTable : public testing::Test {
protected:
    void expect_dropped_and_logged(const Message& message, const std::string& expected) {
        EXPECT_FALSE(table.admits(message));
        const auto records = sink->records();
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records.front().level, logger::Level::Warn);
        EXPECT_NE(records.front().message.find(expected), std::string::npos)
            << records.front().message;
    }

    engine::network::TypeTable table;
    std::shared_ptr<logger::MemorySink> sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped{sink, logger::Level::Trace};
};

} // namespace

TEST_F(TypeTable, StartsUnassigned) {
    EXPECT_FALSE(table.is_assigned());
    EXPECT_FALSE(table.is_known(player_type));
}

TEST_F(TypeTable, KnowsTypesFromConnectAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.is_assigned());
    EXPECT_TRUE(table.is_known(player_type));
    EXPECT_TRUE(table.is_known(enemy_type));
    EXPECT_FALSE(table.is_known(unknown_type));
}

TEST_F(TypeTable, AssignReplacesPreviousTable) {
    table.assign(sample_accept());

    table.assign(ConnectAcceptPayload{.types = {{.type_id = unknown_type, .name = "boss"}}});

    EXPECT_TRUE(table.is_known(unknown_type));
    EXPECT_FALSE(table.is_known(player_type));
}

TEST_F(TypeTable, AdmitsSnapshotWithKnownTypes) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(snapshot_of(player_type, enemy_type)));
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(TypeTable, AdmitsEmptySnapshotAfterAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(SnapshotPayload{.server_tick = 1, .entities = {}}));
}

TEST_F(TypeTable, DropsSnapshotWithOneUnknownType) {
    table.assign(sample_accept());

    expect_dropped_and_logged(snapshot_of(player_type, unknown_type), "unknown type id 99");
}

TEST_F(TypeTable, AdmitsSpawnWithKnownType) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(spawn_of(enemy_type)));
    EXPECT_TRUE(sink->records().empty());
}

TEST_F(TypeTable, DropsSpawnWithUnknownType) {
    table.assign(sample_accept());

    expect_dropped_and_logged(spawn_of(unknown_type), "spawn event with unknown type id 99");
}

TEST_F(TypeTable, AdmitsOtherEventsAfterAccept) {
    table.assign(sample_accept());

    EXPECT_TRUE(table.admits(EventPayload{DestroyEvent{.entity_id = 3}}));
    EXPECT_TRUE(table.admits(EventPayload{SoundEvent{.source_entity_id = 3, .sound_id = 4}}));
}

TEST_F(TypeTable, DropsSnapshotBeforeAccept) {
    expect_dropped_and_logged(snapshot_of(player_type, enemy_type), "before connect_accept");
}

TEST_F(TypeTable, DropsSpawnBeforeAccept) {
    expect_dropped_and_logged(spawn_of(player_type), "before connect_accept");
}

TEST_F(TypeTable, DropsOtherEventsBeforeAccept) {
    expect_dropped_and_logged(EventPayload{DestroyEvent{.entity_id = 3}}, "before connect_accept");
}

TEST_F(TypeTable, AdmitsMessagesWithoutTypeIdsBeforeAccept) {
    EXPECT_TRUE(table.admits(HeartbeatPayload{}));
    EXPECT_TRUE(table.admits(DisconnectPayload{}));
    EXPECT_TRUE(table.admits(sample_accept()));
    EXPECT_TRUE(sink->records().empty());
}
