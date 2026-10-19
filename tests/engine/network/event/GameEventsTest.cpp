#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/event/EventBus.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "protocol/Messages.hpp"

namespace {

using engine::core::event::EventBus;
using engine::network::DestroyEvent;
using engine::network::EntityDestroyed;
using engine::network::EntitySpawned;
using engine::network::EventPayload;
using engine::network::FireEvent;
using engine::network::PlayerLeft;
using engine::network::PlayerLeftEvent;
using engine::network::publish_received;
using engine::network::ShotFired;
using engine::network::SoundEvent;
using engine::network::SoundPlayed;
using engine::network::SpawnEvent;
using engine::network::to_payload;

} // namespace

TEST(GameEvents, SpawnRoundsItsPositionToWireUnits) {
    EXPECT_EQ((to_payload(EntitySpawned{.entity_id = 3, .type_id = 2, .x = 10.6F, .y = -0.4F})),
              (EventPayload{SpawnEvent{.entity_id = 3, .type_id = 2, .x = 11, .y = 0}}));
}

TEST(GameEvents, SpawnClampsPositionsOutsideTheWireRange) {
    EXPECT_EQ((to_payload(EntitySpawned{.entity_id = 3, .type_id = 2, .x = 1e9F, .y = -1e9F})),
              (EventPayload{SpawnEvent{.entity_id = 3, .type_id = 2, .x = 32767, .y = -32768}}));
}

TEST(GameEvents, OtherEventsKeepTheirIds) {
    EXPECT_EQ(to_payload(EntityDestroyed{.entity_id = 9}),
              EventPayload{DestroyEvent{.entity_id = 9}});
    EXPECT_EQ((to_payload(ShotFired{.source_entity_id = 1, .projectile_entity_id = 2})),
              (EventPayload{FireEvent{.source_entity_id = 1, .projectile_entity_id = 2}}));
    EXPECT_EQ((to_payload(SoundPlayed{.source_entity_id = 1, .sound_id = 5})),
              (EventPayload{SoundEvent{.source_entity_id = 1, .sound_id = 5}}));
}

TEST(GameEvents, PublishesEachReceivedEventAsItsGameEvent) {
    EventBus bus;
    std::vector<EntitySpawned> spawned;
    std::vector<std::uint32_t> destroyed;
    std::vector<std::uint32_t> projectiles;
    std::vector<std::uint16_t> sounds;
    std::vector<std::uint32_t> left;
    bus.subscribe<EntitySpawned>([&](const EntitySpawned& event) { spawned.push_back(event); });
    bus.subscribe<EntityDestroyed>(
        [&](const EntityDestroyed& event) { destroyed.push_back(event.entity_id); });
    bus.subscribe<ShotFired>(
        [&](const ShotFired& event) { projectiles.push_back(event.projectile_entity_id); });
    bus.subscribe<SoundPlayed>([&](const SoundPlayed& event) { sounds.push_back(event.sound_id); });
    bus.subscribe<PlayerLeft>([&](const PlayerLeft& event) { left.push_back(event.player_id); });

    publish_received(bus, SpawnEvent{.entity_id = 3, .type_id = 2, .x = -5, .y = 7});
    publish_received(bus, DestroyEvent{.entity_id = 4});
    publish_received(bus, FireEvent{.source_entity_id = 1, .projectile_entity_id = 6});
    publish_received(bus, SoundEvent{.source_entity_id = 1, .sound_id = 8});
    publish_received(bus, PlayerLeftEvent{.player_entity_id = 2});

    ASSERT_EQ(spawned.size(), 1U);
    EXPECT_EQ(spawned.front().entity_id, 3U);
    EXPECT_EQ(spawned.front().type_id, 2);
    EXPECT_FLOAT_EQ(spawned.front().x, -5.0F);
    EXPECT_FLOAT_EQ(spawned.front().y, 7.0F);
    EXPECT_EQ(destroyed, (std::vector<std::uint32_t>{4}));
    EXPECT_EQ(projectiles, (std::vector<std::uint32_t>{6}));
    EXPECT_EQ(sounds, (std::vector<std::uint16_t>{8}));
    EXPECT_EQ(left, (std::vector<std::uint32_t>{2}));
}
