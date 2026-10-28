#include "EventReader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "ValueReader.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/Validation.hpp"

namespace engine::network {

namespace {

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

bool can_hold_arguments(const ByteReader& reader, std::size_t count) noexcept {
    return count <= max_event_arguments && count * event_argument_header_size <= reader.remaining();
}

std::optional<EventArgumentState> read_argument(ByteReader& reader) {
    const auto key_id = reader.read_u16();
    const auto kind = reader.read_u8();
    if (!key_id || !kind) {
        return std::nullopt;
    }
    auto value = read_value(reader, *kind);
    if (!value) {
        return std::nullopt;
    }
    return EventArgumentState{.key_id = *key_id, .value = std::move(*value)};
}

std::optional<EventPayload> read_named(ByteReader& reader) {
    const auto name_id = reader.read_u16();
    const auto count = reader.read_u8();
    if (!name_id || !count || !can_hold_arguments(reader, *count)) {
        return std::nullopt;
    }
    NamedEventState event{.name_id = *name_id, .arguments = {}};
    event.arguments.reserve(*count);
    for (std::size_t index = 0; index < *count; ++index) {
        auto argument = read_argument(reader);
        if (!argument) {
            return std::nullopt;
        }
        event.arguments.push_back(std::move(*argument));
    }
    if (!is_valid(event)) {
        return std::nullopt;
    }
    return event;
}

using EventReader = std::optional<EventPayload> (*)(ByteReader&);

struct EventReaderEntry {
    EventType type;
    EventReader read;
};

constexpr std::array event_readers{
    EventReaderEntry{.type = EventType::sound, .read = read_sound},
    EventReaderEntry{.type = EventType::player_left, .read = read_player_left},
    EventReaderEntry{.type = EventType::music, .read = read_music},
    EventReaderEntry{.type = EventType::named, .read = read_named},
};

} // namespace

std::optional<EventPayload> read_event(ByteReader& reader, std::uint8_t type) {
    const auto entry =
        std::ranges::find(event_readers, static_cast<EventType>(type), &EventReaderEntry::type);
    if (entry == event_readers.end()) {
        return std::nullopt;
    }
    return entry->read(reader);
}

} // namespace engine::network
