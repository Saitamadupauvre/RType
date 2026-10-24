#include "GameEvents.hpp"

#include <variant>

#include "snapshot/SnapshotBuilder.hpp"

namespace engine::network {

namespace {

void publish(core::event::EventBus& events, const SpawnEvent& event) {
    events.publish(EntitySpawned{.entity_id = event.entity_id,
                                 .type_id = event.type_id,
                                 .x = static_cast<float>(event.x),
                                 .y = static_cast<float>(event.y)});
}

void publish(core::event::EventBus& events, const DestroyEvent& event) {
    events.publish(EntityDestroyed{.entity_id = event.entity_id});
}

void publish(core::event::EventBus& events, const FireEvent& event) {
    events.publish(ShotFired{.source_entity_id = event.source_entity_id,
                             .projectile_entity_id = event.projectile_entity_id});
}

void publish(core::event::EventBus& events, const SoundEvent& event) {
    events.publish(
        SoundPlayed{.source_entity_id = event.source_entity_id, .sound_id = event.sound_id});
}

void publish(core::event::EventBus& events, const PlayerLeftEvent& event) {
    events.publish(PlayerLeft{.player_id = event.player_entity_id});
}

void publish(core::event::EventBus& events, const MusicEvent& event) {
    events.publish(MusicChanged{.music_id = event.music_id});
}

} // namespace

EventPayload to_payload(const EntitySpawned& event) noexcept {
    return SpawnEvent{.entity_id = event.entity_id,
                      .type_id = event.type_id,
                      .x = to_wire_coordinate(event.x),
                      .y = to_wire_coordinate(event.y)};
}

EventPayload to_payload(const EntityDestroyed& event) noexcept {
    return DestroyEvent{.entity_id = event.entity_id};
}

EventPayload to_payload(const ShotFired& event) noexcept {
    return FireEvent{.source_entity_id = event.source_entity_id,
                     .projectile_entity_id = event.projectile_entity_id};
}

EventPayload to_payload(const SoundPlayed& event) noexcept {
    return SoundEvent{.source_entity_id = event.source_entity_id, .sound_id = event.sound_id};
}

EventPayload to_payload(const MusicChanged& event) noexcept {
    return MusicEvent{.music_id = event.music_id};
}

void publish_received(core::event::EventBus& events, const EventPayload& event) {
    std::visit([&events](const auto& payload) { publish(events, payload); }, event);
}

} // namespace engine::network
