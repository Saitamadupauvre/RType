#include "Validation.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <variant>

#include "engine/core/Keys.hpp"

#include "FieldWire.hpp"
#include "Protocol.hpp"

namespace engine::network {

namespace {

static_assert(core::key_count <= input_key_bytes * 8);
static_assert(core::mouse_button_count <= 8);

bool is_bit_set(std::span<const std::uint8_t> bits, std::size_t index) noexcept {
    return ((static_cast<unsigned int>(bits[index / 8]) >> (index % 8)) & 1U) != 0;
}

bool holds_only_known_keys(const InputPayload& payload) noexcept {
    if (is_bit_set(payload.keys, static_cast<std::size_t>(core::Key::Unknown))) {
        return false;
    }
    for (std::size_t index = core::key_count; index < input_key_bytes * 8; ++index) {
        if (is_bit_set(payload.keys, index)) {
            return false;
        }
    }
    return true;
}

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

bool fits_event(const core::FieldValue& value) {
    if (!fits_wire(value)) {
        return false;
    }
    if (const auto* text = std::get_if<std::string>(&value)) {
        return text->size() <= max_event_text_length;
    }
    if (const auto* asset = std::get_if<core::AssetName>(&value)) {
        return asset->path.size() <= max_event_text_length;
    }
    return true;
}

bool has_unique_keys(const NamedEventState& event) {
    std::unordered_set<std::uint16_t> ids;
    return std::ranges::all_of(event.arguments, [&ids](const EventArgumentState& argument) {
        return ids.insert(argument.key_id).second;
    });
}

constexpr std::size_t largest_named_event_size =
    named_event_header_size +
    (max_event_arguments * (event_argument_header_size + 1 + max_event_text_length));

static_assert(largest_named_event_size <= max_payload_size);

std::size_t entities_size(const SnapshotPayload& payload) noexcept {
    std::size_t size = 0;
    for (const EntityState& entity : payload.entities) {
        size += wire_size(entity);
    }
    return size;
}

} // namespace

bool is_valid(const InputPayload& payload) noexcept {
    return holds_only_known_keys(payload) &&
           (static_cast<unsigned int>(payload.mouse_buttons) >> core::mouse_button_count) == 0;
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

bool is_valid(const NamedEventState& event) {
    return event.arguments.size() <= max_event_arguments &&
           std::ranges::all_of(
               event.arguments,
               [](const EventArgumentState& argument) { return fits_event(argument.value); }) &&
           has_unique_keys(event);
}

bool is_valid(const EventPayload& payload) {
    const auto* named = std::get_if<NamedEventState>(&payload);
    return named == nullptr || is_valid(*named);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::server_full);
}

} // namespace engine::network
