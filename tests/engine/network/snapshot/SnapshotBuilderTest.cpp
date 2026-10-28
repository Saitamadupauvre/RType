#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/StringTable.hpp"

#include "TestFields.hpp"
#include "encoder/PayloadEncoder.hpp"
#include "protocol/Protocol.hpp"
#include "snapshot/ReplicatedFields.hpp"
#include "snapshot/SnapshotBuilder.hpp"
#include "snapshot/SnapshotReceiver.hpp"

namespace {

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::FieldValue;
using engine::core::Vec2;
using engine::network::build_snapshots;
using engine::network::encode_payload;
using engine::network::EntityState;
using engine::network::FieldState;
using engine::network::max_payload_size;
using engine::network::Replicated;
using engine::network::ReplicatedFields;
using engine::network::SnapshotReceiver;
using engine::network::StringCategory;
using engine::network::StringTable;
using test_fields::Body;
using test_fields::Label;

constexpr std::uint32_t sample_tick = 42;

const EntityState* find_state(const std::vector<EntityState>& states, Entity entity) {
    const auto found = std::ranges::find(states, entity.index(), &EntityState::entity_id);
    return found == states.end() ? nullptr : &*found;
}

class SnapshotBuilder : public testing::Test {
protected:
    SnapshotBuilder() {
        world.register_component<Replicated>();
        test_fields::register_components(world);
    }

    Entity spawn_body(float x, float y) {
        const auto entity = world.spawn();
        world.add(entity, Replicated{.type_id = 1});
        world.add(entity, Body{.pos = {x, y}, .spin = 0.F});
        return entity;
    }

    [[nodiscard]] std::uint16_t id_of(const std::string& name) const {
        return strings.id_of(StringCategory::field, name).value_or(0);
    }

    EntityManager world;
    engine::core::FieldRegistry fields = test_fields::registry();
    StringTable strings;
    ReplicatedFields replicated{fields, strings};
};

} // namespace

TEST_F(SnapshotBuilder, WorldWithoutReplicatedComponentGivesOneEmptySnapshot) {
    EntityManager empty;

    const auto snapshots = build_snapshots(empty, replicated, sample_tick);

    ASSERT_EQ(snapshots.size(), 1U);
    EXPECT_EQ(snapshots.front().server_tick, sample_tick);
    EXPECT_TRUE(snapshots.front().entities.empty());
}

TEST_F(SnapshotBuilder, SendsOnlyReplicatedEntities) {
    const auto first = spawn_body(10.F, -20.F);
    const auto hidden = world.spawn();
    world.add(hidden, Body{.pos = {1.F, 1.F}, .spin = 0.F});

    const auto snapshots = build_snapshots(world, replicated, sample_tick);

    ASSERT_EQ(snapshots.size(), 1U);
    const auto& states = snapshots.front().entities;
    ASSERT_EQ(states.size(), 1U);
    EXPECT_NE(find_state(states, first), nullptr);
    EXPECT_EQ(find_state(states, hidden), nullptr);
}

TEST_F(SnapshotBuilder, SendsTheReplicatedFieldsTheEntityOwns) {
    const auto entity = world.spawn();
    world.add(entity, Replicated{.type_id = 2, .owner = 3});
    world.add(entity, Body{.pos = {5.F, 6.F}, .spin = 0.25F});

    const auto snapshots = build_snapshots(world, replicated, sample_tick);

    const auto* state = find_state(snapshots.front().entities, entity);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(*state,
              (EntityState{
                  .entity_id = entity.index(),
                  .type_id = 2,
                  .fields = {{.field_id = id_of("Body.pos"), .value = Vec2{5.F, 6.F}},
                             {.field_id = id_of("Body.spin"), .value = 0.25F},
                             {.field_id = id_of("Replicated.owner"), .value = std::int64_t{3}}}}));
}

TEST_F(SnapshotBuilder, LeavesOutFieldsThatAreNotReplicated) {
    const auto entity = world.spawn();
    world.add(entity, Replicated{.type_id = 1});
    world.add(entity, Label{.text = "boss", .hidden = true});

    const auto snapshots = build_snapshots(world, replicated, sample_tick);

    const auto* state = find_state(snapshots.front().entities, entity);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->fields,
              (std::vector<FieldState>{
                  {.field_id = id_of("Label.text"), .value = FieldValue{std::string("boss")}},
                  {.field_id = id_of("Replicated.owner"), .value = std::int64_t{0}}}));
}

TEST_F(SnapshotBuilder, SplitsLargeWorldIntoPacketsUnderTheSizeLimit) {
    constexpr std::size_t entity_count = 200;
    for (std::size_t index = 0; index < entity_count; ++index) {
        spawn_body(static_cast<float>(index), 0.F);
    }

    const auto snapshots = build_snapshots(world, replicated, sample_tick);

    ASSERT_EQ(snapshots.size(), 5U);
    std::set<std::uint32_t> ids;
    for (const auto& snapshot : snapshots) {
        EXPECT_EQ(snapshot.server_tick, sample_tick);
        const auto encoded = encode_payload(snapshot);
        ASSERT_TRUE(encoded.has_value());
        EXPECT_LE(encoded->size(), max_payload_size);
        for (const auto& state : snapshot.entities) {
            ids.insert(state.entity_id);
        }
    }
    EXPECT_EQ(ids.size(), entity_count);
}

TEST_F(SnapshotBuilder, SplitSnapshotIsReassembledByTheReceiver) {
    constexpr std::size_t entity_count = 100;
    for (std::size_t index = 0; index < entity_count; ++index) {
        spawn_body(static_cast<float>(index), 0.F);
    }
    SnapshotReceiver receiver;

    const auto snapshots = build_snapshots(world, replicated, sample_tick);
    EXPECT_GT(snapshots.size(), 1U);
    for (const auto& snapshot : snapshots) {
        EXPECT_TRUE(receiver.receive(snapshot));
    }

    EXPECT_EQ(receiver.server_tick(), sample_tick);
    EXPECT_EQ(receiver.entities().size(), entity_count);
}
