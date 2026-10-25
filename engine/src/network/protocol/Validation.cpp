#include "Validation.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "Protocol.hpp"

namespace engine::network {

namespace {

constexpr std::uint8_t known_action_flags = 0x01;

bool is_unit_axis(std::int8_t value) noexcept { return value >= -1 && value <= 1; }

bool has_valid_name_length(const TypeEntry& entry) noexcept {
    return !entry.name.empty() && entry.name.size() <= max_type_name_length;
}

std::size_t encoded_size(const std::vector<TypeEntry>& entries) noexcept {
    std::size_t size = 0;
    for (const auto& entry : entries) {
        size += min_type_entry_size + entry.name.size();
    }
    return size;
}

std::size_t encoded_size(const ConnectAcceptPayload& payload) noexcept {
    return connect_accept_prefix_size + encoded_size(payload.types) + encoded_size(payload.sounds) +
           encoded_size(payload.musics);
}

bool is_valid_table(const std::vector<TypeEntry>& entries) {
    std::unordered_set<std::uint16_t> type_ids;
    std::unordered_set<std::string_view> names;
    for (const auto& entry : entries) {
        if (!has_valid_name_length(entry) || !type_ids.insert(entry.type_id).second ||
            !names.insert(entry.name).second) {
            return false;
        }
    }
    return true;
}

bool is_valid_music_table(const ConnectAcceptPayload& payload) {
    const auto has_id = [](std::uint16_t id) {
        return [id](const TypeEntry& entry) { return entry.type_id == id; };
    };
    return std::ranges::none_of(payload.musics, has_id(no_music)) &&
           (payload.current_music == no_music ||
            std::ranges::any_of(payload.musics, has_id(payload.current_music)));
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
    return encoded_size(payload) <= max_payload_size && is_valid_table(payload.types) &&
           is_valid_table(payload.sounds) && is_valid_table(payload.musics) &&
           is_valid_music_table(payload);
}

bool is_valid(const DisconnectPayload& payload) noexcept {
    return static_cast<std::uint8_t>(payload.reason) <=
           static_cast<std::uint8_t>(DisconnectReason::server_full);
}

} // namespace engine::network
