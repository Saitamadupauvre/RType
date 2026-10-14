#include "Validation.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <unordered_set>

#include "Protocol.hpp"

namespace engine::network {

namespace {

constexpr std::uint8_t known_action_flags = 0x01;

bool is_unit_axis(std::int8_t value) noexcept { return value >= -1 && value <= 1; }

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

} // namespace

bool is_valid(const InputPayload& payload) noexcept {
    return (payload.action_flags & ~known_action_flags) == 0 && is_unit_axis(payload.move_x) &&
           is_unit_axis(payload.move_y);
}

bool is_valid(const SnapshotPayload& payload) noexcept {
    return payload.entities.size() <= max_snapshot_entities;
}

bool is_valid(const ConnectAcceptPayload& payload) {
    return std::ranges::all_of(payload.types, has_valid_name_length) &&
           encoded_size(payload) <= max_payload_size && has_unique_types(payload);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::protocol_error);
}

} // namespace engine::network
