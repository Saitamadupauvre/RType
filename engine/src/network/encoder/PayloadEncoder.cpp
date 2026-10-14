#include "Payloads.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>

#include "ByteReader.hpp"
#include "ByteWriter.hpp"

namespace engine::network {

namespace {

constexpr std::uint8_t known_action_flags = 0x01;

bool is_unit_axis(std::int8_t value) noexcept { return value >= -1 && value <= 1; }

bool is_valid(const InputPayload& payload) noexcept {
    return (payload.action_flags & ~known_action_flags) == 0 && is_unit_axis(payload.move_x) &&
           is_unit_axis(payload.move_y);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::protocol_error);
}

void write_event_type(ByteWriter& writer, EventType type) {
    writer.write_u8(static_cast<std::uint8_t>(type));
}

void write_event(ByteWriter& writer, const SpawnEvent& event) {
    write_event_type(writer, EventType::spawn);
    writer.write_u32(event.entity_id);
    writer.write_u16(event.type_id);
    writer.write_i16(event.x);
    writer.write_i16(event.y);
}

void write_event(ByteWriter& writer, const DestroyEvent& event) {
    write_event_type(writer, EventType::destroy);
    writer.write_u32(event.entity_id);
}

void write_event(ByteWriter& writer, const FireEvent& event) {
    write_event_type(writer, EventType::fire);
    writer.write_u32(event.source_entity_id);
    writer.write_u32(event.projectile_entity_id);
}

void write_event(ByteWriter& writer, const SoundEvent& event) {
    write_event_type(writer, EventType::sound);
    writer.write_u32(event.source_entity_id);
    writer.write_u16(event.sound_id);
}

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
};

std::optional<EventPayload> read_event(ByteReader& reader, std::uint8_t type) noexcept {
    const auto entry =
        std::ranges::find(event_readers, static_cast<EventType>(type), &EventReaderEntry::type);
    if (entry == event_readers.end()) {
        return std::nullopt;
    }
    return entry->read(reader);
}

bool is_valid(const SnapshotPayload& payload) noexcept {
    return payload.entities.size() <= max_snapshot_entities;
}

void write_entity(ByteWriter& writer, const EntityState& entity) {
    writer.write_u32(entity.entity_id);
    writer.write_u16(entity.type_id);
    writer.write_i16(entity.x);
    writer.write_i16(entity.y);
    writer.write_i16(entity.vx);
    writer.write_i16(entity.vy);
}

std::optional<EntityState> read_entity(ByteReader& reader) noexcept {
    const auto entity_id = reader.read_u32();
    const auto type_id = reader.read_u16();
    const auto x = reader.read_i16();
    const auto y = reader.read_i16();
    const auto vx = reader.read_i16();
    const auto vy = reader.read_i16();
    if (!entity_id || !type_id || !x || !y || !vx || !vy) {
        return std::nullopt;
    }
    return EntityState{
        .entity_id = *entity_id,
        .type_id = *type_id,
        .x = *x,
        .y = *y,
        .vx = *vx,
        .vy = *vy,
    };
}

bool has_exact_entity_records(const ByteReader& reader, std::size_t count) noexcept {
    return count <= max_snapshot_entities && reader.remaining() == count * entity_record_size;
}

bool has_valid_name_length(const TypeEntry& entry) noexcept {
    return !entry.name.empty() && entry.name.size() <= max_type_name_length;
}

std::size_t encoded_size(const ConnectAcceptPayload& payload) noexcept {
    std::size_t size = connect_accept_prefix_size;
    for (const auto& entry : payload.types) {
        size += min_type_entry_size + entry.name.size();
    }
    return size;
}

bool has_unique_types(const ConnectAcceptPayload& payload) {
    std::unordered_set<std::uint16_t> type_ids;
    std::unordered_set<std::string_view> names;
    for (const auto& entry : payload.types) {
        if (!type_ids.insert(entry.type_id).second || !names.insert(entry.name).second) {
            return false;
        }
    }
    return true;
}

bool is_valid(const ConnectAcceptPayload& payload) {
    return std::ranges::all_of(payload.types, has_valid_name_length) &&
           encoded_size(payload) <= max_payload_size && has_unique_types(payload);
}

void write_type_entry(ByteWriter& writer, const TypeEntry& entry) {
    writer.write_u16(entry.type_id);
    writer.write_u8(static_cast<std::uint8_t>(entry.name.size()));
    for (const char character : entry.name) {
        writer.write_u8(static_cast<std::uint8_t>(character));
    }
}

std::optional<TypeEntry> read_type_entry(ByteReader& reader) {
    const auto type_id = reader.read_u16();
    const auto name_length = reader.read_u8();
    if (!type_id || !name_length) {
        return std::nullopt;
    }

    const auto name = reader.read_bytes(*name_length);
    if (!name) {
        return std::nullopt;
    }
    return TypeEntry{.type_id = *type_id, .name = std::string(name->begin(), name->end())};
}

bool can_hold_type_entries(const ByteReader& reader, std::size_t count) noexcept {
    return count * min_type_entry_size <= reader.remaining();
}

} // namespace

EncodedPayload encode_payload(const InputPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u8(payload.action_flags);
    writer.write_i8(payload.move_x);
    writer.write_i8(payload.move_y);
    return std::move(writer).take();
}

std::optional<InputPayload> decode_input(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto action_flags = reader.read_u8();
    const auto move_x = reader.read_i8();
    const auto move_y = reader.read_i8();
    if (!action_flags || !move_x || !move_y || !reader.at_end()) {
        return std::nullopt;
    }

    const InputPayload input{.action_flags = *action_flags, .move_x = *move_x, .move_y = *move_y};
    if (!is_valid(input)) {
        return std::nullopt;
    }
    return input;
}

EncodedPayload encode_payload(const ConnectRequestPayload& payload) {
    ByteWriter writer;
    writer.write_u32(payload.client_nonce);
    return std::move(writer).take();
}

std::optional<ConnectRequestPayload> decode_connect_request(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto client_nonce = reader.read_u32();
    if (!client_nonce || !reader.at_end()) {
        return std::nullopt;
    }
    return ConnectRequestPayload{.client_nonce = *client_nonce};
}

EncodedPayload encode_payload(const HeartbeatPayload&) { return Bytes{}; }

std::optional<HeartbeatPayload> decode_heartbeat(std::span<const std::uint8_t> payload) {
    if (!payload.empty()) {
        return std::nullopt;
    }
    return HeartbeatPayload{};
}

EncodedPayload encode_payload(const DisconnectPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(payload.reason));
    return std::move(writer).take();
}

std::optional<DisconnectPayload> decode_disconnect(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto reason = reader.read_u8();
    if (!reason || !reader.at_end()) {
        return std::nullopt;
    }

    const DisconnectPayload disconnect{.reason = static_cast<DisconnectReason>(*reason)};
    if (!is_valid(disconnect)) {
        return std::nullopt;
    }
    return disconnect;
}

EncodedPayload encode_payload(const EventPayload& payload) {
    ByteWriter writer;
    std::visit([&writer](const auto& event) { write_event(writer, event); }, payload);
    return std::move(writer).take();
}

std::optional<EventPayload> decode_event(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto type = reader.read_u8();
    if (!type) {
        return std::nullopt;
    }

    auto event = read_event(reader, *type);
    if (!event || !reader.at_end()) {
        return std::nullopt;
    }
    return event;
}

EncodedPayload encode_payload(const SnapshotPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u32(payload.server_tick);
    writer.write_u16(static_cast<std::uint16_t>(payload.entities.size()));
    for (const auto& entity : payload.entities) {
        write_entity(writer, entity);
    }
    return std::move(writer).take();
}

std::optional<SnapshotPayload> decode_snapshot(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto server_tick = reader.read_u32();
    const auto count = reader.read_u16();
    if (!server_tick || !count || !has_exact_entity_records(reader, *count)) {
        return std::nullopt;
    }

    SnapshotPayload snapshot{.server_tick = *server_tick, .entities = {}};
    snapshot.entities.reserve(*count);
    for (std::size_t index = 0; index < *count; ++index) {
        const auto entity = read_entity(reader);
        if (!entity) {
            return std::nullopt;
        }
        snapshot.entities.push_back(*entity);
    }
    return snapshot;
}

EncodedPayload encode_payload(const ConnectAcceptPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u32(payload.client_nonce);
    writer.write_u32(payload.player_entity_id);
    writer.write_u16(static_cast<std::uint16_t>(payload.types.size()));
    for (const auto& entry : payload.types) {
        write_type_entry(writer, entry);
    }
    return std::move(writer).take();
}

std::optional<ConnectAcceptPayload> decode_connect_accept(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto client_nonce = reader.read_u32();
    const auto player_entity_id = reader.read_u32();
    const auto count = reader.read_u16();
    if (!client_nonce || !player_entity_id || !count || !can_hold_type_entries(reader, *count)) {
        return std::nullopt;
    }

    ConnectAcceptPayload accept{
        .client_nonce = *client_nonce,
        .player_entity_id = *player_entity_id,
        .types = {},
    };
    accept.types.reserve(*count);
    for (std::size_t index = 0; index < *count; ++index) {
        auto entry = read_type_entry(reader);
        if (!entry) {
            return std::nullopt;
        }
        accept.types.push_back(std::move(*entry));
    }

    if (!reader.at_end() || !is_valid(accept)) {
        return std::nullopt;
    }
    return accept;
}

} // namespace engine::network
