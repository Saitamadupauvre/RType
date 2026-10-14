#include "PayloadEncoder.hpp"

#include <cstdint>
#include <utility>
#include <variant>

#include "bytes/ByteWriter.hpp"
#include "protocol/Validation.hpp"

namespace engine::network {

namespace {

void write_type_entry(ByteWriter& writer, const TypeEntry& entry) {
    writer.write_u16(entry.type_id);
    writer.write_u8(static_cast<std::uint8_t>(entry.name.size()));
    for (const char character : entry.name) {
        writer.write_u8(static_cast<std::uint8_t>(character));
    }
}

void write_entity(ByteWriter& writer, const EntityState& entity) {
    writer.write_u32(entity.entity_id);
    writer.write_u16(entity.type_id);
    writer.write_i16(entity.x);
    writer.write_i16(entity.y);
    writer.write_i16(entity.vx);
    writer.write_i16(entity.vy);
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
    writer.write_u16(static_cast<std::uint16_t>(payload.types.size()));
    for (const auto& entry : payload.types) {
        write_type_entry(writer, entry);
    }
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
