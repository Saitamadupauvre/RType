#include "Validation.hpp"

#include <cstddef>

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

} // namespace

bool is_valid(const InputPayload& payload) noexcept {
    return (payload.action_flags | known_action_flags) == known_action_flags &&
           is_unit_axis(payload.move_x) && is_unit_axis(payload.move_y);
}

bool is_valid(const SnapshotPayload& payload) noexcept {
    return payload.entities.size() <= max_snapshot_entities;
}

bool is_valid(const ConnectAcceptPayload& payload) {
    return encoded_size(payload) <= max_payload_size && has_known_current_music(payload);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::server_full);
}

} // namespace engine::network
