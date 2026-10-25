#include "EventReader.hpp"

#include <algorithm>
#include <array>

namespace engine::network {

namespace {

std::optional<EventPayload> read_spawn(ByteReader& reader) noexcept {
    const auto entity_id = reader.read_u32();
    const auto type_id = reader.read_u16();
    const auto x = reader.read_i16();
    const auto y = reader.read_i16();
    if (!entity_id || !type_id || !x || !y) {
        return std::nullopt;
    }
    return SpawnEvent{.entity_id = *entity_id, .type_id = *type_id, .x = *x, .y = *y};
}

std::optional<EventPayload> read_destroy(ByteReader& reader) noexcept {
    const auto entity_id = reader.read_u32();
    if (!entity_id) {
        return std::nullopt;
    }
    return DestroyEvent{.entity_id = *entity_id};
}

std::optional<EventPayload> read_fire(ByteReader& reader) noexcept {
    const auto source_entity_id = reader.read_u32();
    const auto projectile_entity_id = reader.read_u32();
    if (!source_entity_id || !projectile_entity_id) {
        return std::nullopt;
    }
    return FireEvent{.source_entity_id = *source_entity_id,
                     .projectile_entity_id = *projectile_entity_id};
}

std::optional<EventPayload> read_sound(ByteReader& reader) noexcept {
    const auto source_entity_id = reader.read_u32();
    const auto sound_id = reader.read_u16();
    if (!source_entity_id || !sound_id) {
        return std::nullopt;
    }
    return SoundEvent{.source_entity_id = *source_entity_id, .sound_id = *sound_id};
}

std::optional<EventPayload> read_player_left(ByteReader& reader) noexcept {
    const auto player_entity_id = reader.read_u32();
    if (!player_entity_id) {
        return std::nullopt;
    }
    return PlayerLeftEvent{.player_entity_id = *player_entity_id};
}

std::optional<EventPayload> read_music(ByteReader& reader) noexcept {
    const auto music_id = reader.read_u16();
    if (!music_id) {
        return std::nullopt;
    }
    return MusicEvent{.music_id = *music_id};
}

using EventReader = std::optional<EventPayload> (*)(ByteReader&) noexcept;

struct EventReaderEntry {
    EventType type;
    EventReader read;
};

constexpr std::array event_readers{
    EventReaderEntry{.type = EventType::spawn, .read = read_spawn},
    EventReaderEntry{.type = EventType::destroy, .read = read_destroy},
    EventReaderEntry{.type = EventType::fire, .read = read_fire},
    EventReaderEntry{.type = EventType::sound, .read = read_sound},
    EventReaderEntry{.type = EventType::player_left, .read = read_player_left},
    EventReaderEntry{.type = EventType::music, .read = read_music},
};

} // namespace

std::optional<EventPayload> read_event(ByteReader& reader, std::uint8_t type) noexcept {
    const auto entry =
        std::ranges::find(event_readers, static_cast<EventType>(type), &EventReaderEntry::type);
    if (entry == event_readers.end()) {
        return std::nullopt;
    }
    return entry->read(reader);
}

} // namespace engine::network
