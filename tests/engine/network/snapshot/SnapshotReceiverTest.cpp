#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "protocol/Messages.hpp"
#include "snapshot/SnapshotReceiver.hpp"

namespace {

using engine::network::EntityState;
using engine::network::SnapshotPayload;
using engine::network::SnapshotReceiver;

EntityState entity(std::uint32_t id, float x = 0) {
    return EntityState{.entity_id = id,
                       .type_id = 1,
                       .fields = {{.field_id = 1, .value = engine::core::Vec2{x, 0}}}};
}

SnapshotPayload snapshot(std::uint32_t tick, std::vector<EntityState> entities) {
    return SnapshotPayload{.server_tick = tick, .entities = std::move(entities)};
}

} // namespace

TEST(SnapshotReceiver, StartsWithoutSnapshot) {
    const SnapshotReceiver receiver;

    EXPECT_FALSE(receiver.server_tick().has_value());
    EXPECT_TRUE(receiver.entities().empty());
}

TEST(SnapshotReceiver, AcceptsFirstSnapshot) {
    SnapshotReceiver receiver;

    EXPECT_TRUE(receiver.receive(snapshot(10, {entity(1), entity(2)})));

    EXPECT_EQ(receiver.server_tick(), 10U);
    EXPECT_EQ(receiver.entities(), (std::vector<EntityState>{entity(1), entity(2)}));
}

TEST(SnapshotReceiver, NewerSnapshotReplacesTheWorld) {
    SnapshotReceiver receiver;
    receiver.receive(snapshot(10, {entity(1), entity(2)}));

    EXPECT_TRUE(receiver.receive(snapshot(11, {entity(3)})));

    EXPECT_EQ(receiver.server_tick(), 11U);
    EXPECT_EQ(receiver.entities(), (std::vector<EntityState>{entity(3)}));
}

TEST(SnapshotReceiver, IgnoresOlderSnapshot) {
    SnapshotReceiver receiver;
    receiver.receive(snapshot(11, {entity(3)}));

    EXPECT_FALSE(receiver.receive(snapshot(10, {entity(1)})));

    EXPECT_EQ(receiver.server_tick(), 11U);
    EXPECT_EQ(receiver.entities(), (std::vector<EntityState>{entity(3)}));
}

TEST(SnapshotReceiver, MergesPacketsOfTheSameTick) {
    SnapshotReceiver receiver;
    receiver.receive(snapshot(10, {entity(1)}));

    EXPECT_TRUE(receiver.receive(snapshot(10, {entity(2)})));

    EXPECT_EQ(receiver.entities(), (std::vector<EntityState>{entity(1), entity(2)}));
}

TEST(SnapshotReceiver, DuplicatedPacketDoesNotDuplicateEntities) {
    SnapshotReceiver receiver;
    receiver.receive(snapshot(10, {entity(1, 5)}));

    receiver.receive(snapshot(10, {entity(1, 6)}));

    EXPECT_EQ(receiver.entities(), (std::vector<EntityState>{entity(1, 6)}));
}

TEST(SnapshotReceiver, AcceptsSnapshotAcrossTickWrap) {
    SnapshotReceiver receiver;
    receiver.receive(snapshot(0xFFFF'FFFF, {entity(1)}));

    EXPECT_TRUE(receiver.receive(snapshot(0, {entity(2)})));

    EXPECT_EQ(receiver.server_tick(), 0U);
}
