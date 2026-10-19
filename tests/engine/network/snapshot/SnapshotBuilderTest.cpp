#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/network/Replicated.hpp"

#include "encoder/PayloadEncoder.hpp"
#include "protocol/Protocol.hpp"
#include "snapshot/SnapshotBuilder.hpp"
#include "snapshot/SnapshotReceiver.hpp"

namespace {

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Velocity;
using engine::network::build_snapshots;
using engine::network::encode_payload;
using engine::network::EntityState;
using engine::network::max_payload_size;
using engine::network::max_snapshot_entities;
using engine::network::Replicated;
using engine::network::SnapshotReceiver;
using engine::network::to_wire_coordinate;

constexpr std::uint32_t sample_tick = 42;

EntityManager registry_with_components() {
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    registry.register_component<Velocity>();
    return registry;
}

Entity spawn_replicated(EntityManager& registry, float x, float y) {
    const auto entity = registry.spawn();
    registry.add(entity, Replicated{.type_id = 1});
    registry.add(entity, Position{x, y});
    return entity;
}

const EntityState* find_state(const std::vector<EntityState>& states, Entity entity) {
    const auto found = std::ranges::find(states, entity.index(), &EntityState::entity_id);
    return found == states.end() ? nullptr : &*found;
}

} // namespace

TEST(SnapshotBuilder, RoundsAndClampsCoordinatesToSigned16Bits) {
    EXPECT_EQ(to_wire_coordinate(1.4F), 1);
    EXPECT_EQ(to_wire_coordinate(1.6F), 2);
    EXPECT_EQ(to_wire_coordinate(-1.5F), -2);
    EXPECT_EQ(to_wire_coordinate(40'000.0F), std::numeric_limits<std::int16_t>::max());
    EXPECT_EQ(to_wire_coordinate(-40'000.0F), std::numeric_limits<std::int16_t>::min());
    EXPECT_EQ(to_wire_coordinate(std::numeric_limits<float>::infinity()),
              std::numeric_limits<std::int16_t>::max());
    EXPECT_EQ(to_wire_coordinate(std::numeric_limits<float>::quiet_NaN()), 0);
}

TEST(SnapshotBuilder, RegistryWithoutReplicatedComponentGivesOneEmptySnapshot) {
    EntityManager registry;

    const auto snapshots = build_snapshots(registry, sample_tick);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.front().server_tick, sample_tick);
    EXPECT_TRUE(snapshots.front().entities.empty());
}

TEST(SnapshotBuilder, SnapshotMatchesReplicatedEntities) {
    auto registry = registry_with_components();
    const auto moving = spawn_replicated(registry, 10.0F, -20.0F);
    registry.add(moving, Velocity{3.0F, -4.0F});
    const auto still = registry.spawn();
    registry.add(still, Replicated{.type_id = 2});
    registry.add(still, Position{5.0F, 6.0F});
    const auto hidden = registry.spawn();
    registry.add(hidden, Position{1.0F, 1.0F});

    const auto snapshots = build_snapshots(registry, sample_tick);

    ASSERT_EQ(snapshots.size(), 1U);
    const auto& states = snapshots.front().entities;
    ASSERT_EQ(states.size(), 2U);
    EXPECT_EQ(
        *find_state(states, moving),
        (EntityState{
            .entity_id = moving.index(), .type_id = 1, .x = 10, .y = -20, .vx = 3, .vy = -4}));
    EXPECT_EQ(
        *find_state(states, still),
        (EntityState{.entity_id = still.index(), .type_id = 2, .x = 5, .y = 6, .vx = 0, .vy = 0}));
    EXPECT_EQ(find_state(states, hidden), nullptr);
}

TEST(SnapshotBuilder, WorksWhenVelocityIsNotRegistered) {
    EntityManager registry;
    registry.register_component<Replicated>();
    registry.register_component<Position>();
    const auto entity = spawn_replicated(registry, 7.0F, 8.0F);

    const auto snapshots = build_snapshots(registry, sample_tick);

    ASSERT_EQ(snapshots.front().entities.size(), 1U);
    EXPECT_EQ(find_state(snapshots.front().entities, entity)->vx, 0);
}

TEST(SnapshotBuilder, SplitsLargeWorldIntoPacketsUnderTheSizeLimit) {
    auto registry = registry_with_components();
    constexpr std::size_t entity_count = 200;
    for (std::size_t index = 0; index < entity_count; ++index) {
        spawn_replicated(registry, static_cast<float>(index), 0.0F);
    }

    const auto snapshots = build_snapshots(registry, sample_tick);

    ASSERT_EQ(snapshots.size(), 3U);
    std::set<std::uint32_t> ids;
    for (const auto& snapshot : snapshots) {
        EXPECT_EQ(snapshot.server_tick, sample_tick);
        EXPECT_LE(snapshot.entities.size(), max_snapshot_entities);
        const auto encoded = encode_payload(snapshot);
        ASSERT_TRUE(encoded.has_value());
        EXPECT_LE(encoded->size(), max_payload_size);
        for (const auto& state : snapshot.entities) {
            ids.insert(state.entity_id);
        }
    }
    EXPECT_EQ(ids.size(), entity_count);
}

TEST(SnapshotBuilder, SplitSnapshotIsReassembledByTheReceiver) {
    auto registry = registry_with_components();
    constexpr std::size_t entity_count = max_snapshot_entities * 2 + 1;
    for (std::size_t index = 0; index < entity_count; ++index) {
        spawn_replicated(registry, static_cast<float>(index), 0.0F);
    }
    SnapshotReceiver receiver;

    for (const auto& snapshot : build_snapshots(registry, sample_tick)) {
        EXPECT_TRUE(receiver.receive(snapshot));
    }

    EXPECT_EQ(receiver.server_tick(), sample_tick);
    EXPECT_EQ(receiver.entities().size(), entity_count);
}
