#include "PayloadEncoder.hpp"

#include <bit>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "bytes/ByteWriter.hpp"
#include "protocol/FieldWire.hpp"
#include "protocol/Validation.hpp"

namespace engine::network {

namespace {

void write_text(ByteWriter& writer, const std::string& text) {
    writer.write_u8(static_cast<std::uint8_t>(text.size()));
    for (const char character : text) {
        writer.write_u8(static_cast<std::uint8_t>(character));
    }
}

void write_string_entry(ByteWriter& writer, const StringEntry& entry) {
    writer.write_u8(static_cast<std::uint8_t>(entry.category));
    writer.write_u16(entry.id);
    write_text(writer, entry.name);
}

void write_string_table(ByteWriter& writer, const StringTable& table) {
    writer.write_u16(static_cast<std::uint16_t>(table.entries().size()));
    for (const auto& entry : table.entries()) {
        write_string_entry(writer, entry);
    }
}

void write_value(ByteWriter& writer, float value) {
    writer.write_u32(std::bit_cast<std::uint32_t>(value));
}

void write_value(ByteWriter& writer, std::int64_t value) {
    writer.write_u32(static_cast<std::uint32_t>(to_wire_int(value)));
}

void write_value(ByteWriter& writer, bool value) { writer.write_u8(value ? 1 : 0); }

void write_value(ByteWriter& writer, const std::string& value) { write_text(writer, value); }

void write_value(ByteWriter& writer, const core::Vec2& value) {
    writer.write_i16(to_wire_coordinate(value.x));
    writer.write_i16(to_wire_coordinate(value.y));
}

void write_value(ByteWriter& writer, const core::Rgba& value) {
    writer.write_u8(value.r);
    writer.write_u8(value.g);
    writer.write_u8(value.b);
    writer.write_u8(value.a);
}

void write_value(ByteWriter& writer, const core::AssetName& value) {
    write_text(writer, value.path);
}

void write_value(ByteWriter&, const core::FieldList&) {}

void write_field(ByteWriter& writer, const FieldState& field) {
    writer.write_u16(field.field_id);
    writer.write_u8(static_cast<std::uint8_t>(core::kind_of(field.value)));
    std::visit([&writer](const auto& value) { write_value(writer, value); }, field.value);
}

void write_entity(ByteWriter& writer, const EntityState& entity) {
    writer.write_u32(entity.entity_id);
    writer.write_u16(entity.type_id);
    writer.write_u8(static_cast<std::uint8_t>(entity.fields.size()));
    for (const FieldState& field : entity.fields) {
        write_field(writer, field);
    }
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

void write_event(ByteWriter& writer, const PlayerLeftEvent& event) {
    write_event_type(writer, EventType::player_left);
    writer.write_u32(event.player_entity_id);
}

void write_event(ByteWriter& writer, const MusicEvent& event) {
    write_event_type(writer, EventType::music);
    writer.write_u16(event.music_id);
}

} // namespace

EncodedPayload encode_payload(const ConnectRequestPayload& payload) {
    ByteWriter writer;
    writer.write_u32(payload.client_nonce);
    return std::move(writer).take();
}

EncodedPayload encode_payload(const ConnectAcceptPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u32(payload.client_nonce);
    writer.write_u32(payload.player_entity_id);
    write_string_table(writer, payload.strings);
    writer.write_u16(payload.current_music);
    return std::move(writer).take();
}

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

EncodedPayload encode_payload(const EventPayload& payload) {
    ByteWriter writer;
    std::visit([&writer](const auto& event) { write_event(writer, event); }, payload);
    return std::move(writer).take();
}

EncodedPayload encode_payload(const HeartbeatPayload&) { return Bytes{}; }

EncodedPayload encode_payload(const DisconnectPayload& payload) {
    if (!is_valid(payload)) {
        return std::nullopt;
    }

    ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(payload.reason));
    return std::move(writer).take();
}

} // namespace engine::network
