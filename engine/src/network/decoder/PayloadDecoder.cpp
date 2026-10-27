#include "PayloadDecoder.hpp"

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

bool has_exact_entity_records(const ByteReader& reader, std::size_t count) noexcept {
    return count <= max_snapshot_entities && reader.remaining() == count * entity_record_size;
}

std::optional<EntityState> read_entity(ByteReader& reader) noexcept {
    const auto entity_id = reader.read_u32();
    const auto type_id = reader.read_u16();
    const auto x = reader.read_i16();
    const auto y = reader.read_i16();
    const auto vx = reader.read_i16();
    const auto vy = reader.read_i16();
    const auto owner = reader.read_u8();
    const auto r = reader.read_u8();
    const auto g = reader.read_u8();
    const auto b = reader.read_u8();
    const auto a = reader.read_u8();
    if (!entity_id || !type_id || !x || !y || !vx || !vy || !owner || !r || !g || !b || !a) {
        return std::nullopt;
    }
    return EntityState{
        .entity_id = *entity_id,
        .type_id = *type_id,
        .x = *x,
        .y = *y,
        .vx = *vx,
        .vy = *vy,
        .owner = *owner,
        .r = *r,
        .g = *g,
        .b = *b,
        .a = *a,
    };
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
