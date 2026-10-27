#include "Validation.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_set>

#include "FieldWire.hpp"
#include "Protocol.hpp"

namespace engine::network {

namespace {

constexpr std::uint8_t known_action_flags = 0x01;

bool is_unit_axis(std::int8_t value) noexcept { return value >= -1 && value <= 1; }

std::size_t encoded_size(const ConnectAcceptPayload& payload) noexcept {
    std::size_t size = connect_accept_prefix_size;
    for (const StringEntry& entry : payload.strings.entries()) {
        size += min_string_entry_size + entry.name.size();
    }
    return size;
}

bool has_known_current_music(const ConnectAcceptPayload& payload) {
    return payload.current_music == no_music ||
           payload.strings.name_of(StringCategory::music, payload.current_music) != nullptr;
}

bool has_unique_fields(const EntityState& entity) {
    std::unordered_set<std::uint16_t> ids;
    return std::ranges::all_of(entity.fields, [&ids](const FieldState& field) {
        return ids.insert(field.field_id).second;
    });
}

bool is_valid(const EntityState& entity) {
    return entity.fields.size() <= max_entity_fields &&
           std::ranges::all_of(entity.fields,
                               [](const FieldState& field) { return fits_wire(field.value); }) &&
           has_unique_fields(entity);
}

std::size_t entities_size(const SnapshotPayload& payload) noexcept {
    std::size_t size = 0;
    for (const EntityState& entity : payload.entities) {
        size += wire_size(entity);
    }
    return size;
}

} // namespace

bool is_valid(const InputPayload& payload) noexcept {
    return (payload.action_flags | known_action_flags) == known_action_flags &&
           is_unit_axis(payload.move_x) && is_unit_axis(payload.move_y);
}

bool is_valid(const SnapshotPayload& payload) {
    return payload.entities.size() <= std::numeric_limits<std::uint16_t>::max() &&
           std::ranges::all_of(payload.entities,
                               [](const EntityState& entity) { return is_valid(entity); }) &&
           entities_size(payload) <= max_snapshot_entity_bytes;
}

bool is_valid(const ConnectAcceptPayload& payload) {
    return encoded_size(payload) <= max_payload_size && has_known_current_music(payload);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::server_full);
}

} // namespace engine::network
