#include "PayloadDecoder.hpp"

#include <bit>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "EventReader.hpp"
#include "bytes/ByteReader.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/Validation.hpp"

namespace engine::network {

namespace {

bool can_hold_string_entries(const ByteReader& reader, std::size_t count) noexcept {
    return count * min_string_entry_size <= reader.remaining();
}

std::optional<StringEntry> read_string_entry(ByteReader& reader) {
    const auto category = reader.read_u8();
    const auto id = reader.read_u16();
    const auto name_length = reader.read_u8();
    if (!category || *category >= string_category_count || !id || !name_length) {
        return std::nullopt;
    }

    const auto name = reader.read_bytes(*name_length);
    if (!name) {
        return std::nullopt;
    }
    return StringEntry{.category = static_cast<StringCategory>(*category),
                       .id = *id,
                       .name = std::string(name->begin(), name->end())};
}

std::optional<StringTable> read_string_table(ByteReader& reader) {
    const auto count = reader.read_u16();
    if (!count || !can_hold_string_entries(reader, *count)) {
        return std::nullopt;
    }
    StringTable table;
    for (std::size_t index = 0; index < *count; ++index) {
        auto entry = read_string_entry(reader);
        if (!entry || !table.insert(std::move(*entry))) {
            return std::nullopt;
        }
    }
    return table;
}

bool can_hold_entities(const ByteReader& reader, std::size_t count) noexcept {
    return count * entity_header_size <= reader.remaining();
}

std::optional<std::string> read_text(ByteReader& reader) {
    const auto length = reader.read_u8();
    const auto bytes = length ? reader.read_bytes(*length) : std::nullopt;
    if (!bytes) {
        return std::nullopt;
    }
    return std::string(bytes->begin(), bytes->end());
}

std::optional<core::FieldValue> read_float(ByteReader& reader) {
    const auto bits = reader.read_u32();
    if (!bits) {
        return std::nullopt;
    }
    const auto value = std::bit_cast<float>(*bits);
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

std::optional<core::FieldValue> read_int(ByteReader& reader) {
    const auto bits = reader.read_u32();
    if (!bits) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(static_cast<std::int32_t>(*bits));
}

std::optional<core::FieldValue> read_bool(ByteReader& reader) {
    const auto byte = reader.read_u8();
    if (!byte || *byte > 1) {
        return std::nullopt;
    }
    return *byte == 1;
}

std::optional<core::FieldValue> read_vec2(ByteReader& reader) {
    const auto x = reader.read_i16();
    const auto y = reader.read_i16();
    if (!x || !y) {
        return std::nullopt;
    }
    return core::Vec2{static_cast<float>(*x), static_cast<float>(*y)};
}

std::optional<core::FieldValue> read_color(ByteReader& reader) {
    const auto r = reader.read_u8();
    const auto g = reader.read_u8();
    const auto b = reader.read_u8();
    const auto a = reader.read_u8();
    if (!r || !g || !b || !a) {
        return std::nullopt;
    }
    return core::Rgba{.r = *r, .g = *g, .b = *b, .a = *a};
}

std::optional<core::FieldValue> read_string(ByteReader& reader) {
    auto text = read_text(reader);
    if (!text) {
        return std::nullopt;
    }
    return core::FieldValue{std::move(*text)};
}

std::optional<core::FieldValue> read_asset(ByteReader& reader) {
    auto path = read_text(reader);
    if (!path) {
        return std::nullopt;
    }
    return core::AssetName{std::move(*path)};
}

std::optional<core::FieldValue> read_value(ByteReader& reader, std::uint8_t kind) {
    switch (static_cast<core::FieldKind>(kind)) {
    case core::FieldKind::Float:
        return read_float(reader);
    case core::FieldKind::Int:
        return read_int(reader);
    case core::FieldKind::Bool:
        return read_bool(reader);
    case core::FieldKind::String:
        return read_string(reader);
    case core::FieldKind::Vec2:
        return read_vec2(reader);
    case core::FieldKind::Color:
        return read_color(reader);
    case core::FieldKind::Asset:
        return read_asset(reader);
    default:
        return std::nullopt;
    }
}

std::optional<FieldState> read_field(ByteReader& reader) {
    const auto field_id = reader.read_u16();
    const auto kind = reader.read_u8();
    if (!field_id || !kind) {
        return std::nullopt;
    }
    auto value = read_value(reader, *kind);
    if (!value) {
        return std::nullopt;
    }
    return FieldState{.field_id = *field_id, .value = std::move(*value)};
}

std::optional<EntityState> read_entity(ByteReader& reader) {
    const auto entity_id = reader.read_u32();
    const auto type_id = reader.read_u16();
    const auto field_count = reader.read_u8();
    if (!entity_id || !type_id || !field_count) {
        return std::nullopt;
    }
    EntityState entity{.entity_id = *entity_id, .type_id = *type_id, .fields = {}};
    entity.fields.reserve(*field_count);
    for (std::size_t index = 0; index < *field_count; ++index) {
        auto field = read_field(reader);
        if (!field) {
            return std::nullopt;
        }
        entity.fields.push_back(std::move(*field));
    }
    return entity;
}

} // namespace

std::optional<ConnectRequestPayload> decode_connect_request(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto client_nonce = reader.read_u32();
    if (!client_nonce || !reader.at_end()) {
        return std::nullopt;
    }
    return ConnectRequestPayload{.client_nonce = *client_nonce};
}

std::optional<ConnectAcceptPayload> decode_connect_accept(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto client_nonce = reader.read_u32();
    const auto player_entity_id = reader.read_u32();
    if (!client_nonce || !player_entity_id) {
        return std::nullopt;
    }
    auto strings = read_string_table(reader);
    const auto current_music = strings ? reader.read_u16() : std::nullopt;
    if (!strings || !current_music) {
        return std::nullopt;
    }

    ConnectAcceptPayload accept{
        .client_nonce = *client_nonce,
        .player_entity_id = *player_entity_id,
        .strings = std::move(*strings),
        .current_music = *current_music,
    };
    if (!reader.at_end() || !is_valid(accept)) {
        return std::nullopt;
    }
    return accept;
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

std::optional<SnapshotPayload> decode_snapshot(std::span<const std::uint8_t> payload) {
    ByteReader reader(payload);
    const auto server_tick = reader.read_u32();
    const auto count = reader.read_u16();
    if (!server_tick || !count || !can_hold_entities(reader, *count)) {
        return std::nullopt;
    }

    SnapshotPayload snapshot{.server_tick = *server_tick, .entities = {}};
    snapshot.entities.reserve(*count);
    for (std::size_t index = 0; index < *count; ++index) {
        auto entity = read_entity(reader);
        if (!entity) {
            return std::nullopt;
        }
        snapshot.entities.push_back(std::move(*entity));
    }
    if (!reader.at_end() || !is_valid(snapshot)) {
        return std::nullopt;
    }
    return snapshot;
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

std::optional<HeartbeatPayload> decode_heartbeat(std::span<const std::uint8_t> payload) {
    if (!payload.empty()) {
        return std::nullopt;
    }
    return HeartbeatPayload{};
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

} // namespace engine::network
