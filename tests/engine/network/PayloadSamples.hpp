#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "encoder/PayloadEncoder.hpp"

namespace payload_samples {

template <typename Payload> std::vector<std::uint8_t> encoded(const Payload& payload) {
    auto bytes = engine::network::encode_payload(payload);
    EXPECT_TRUE(bytes.has_value());
    return bytes.value_or(std::vector<std::uint8_t>{});
}

inline std::array<engine::network::EventPayload, 6> sample_events() {
    return {
        engine::network::SpawnEvent{
            .entity_id = 0x0102'0304, .type_id = 0x0506, .x = -2, .y = 0x0708},
        engine::network::DestroyEvent{.entity_id = 0x0102'0304},
        engine::network::FireEvent{.source_entity_id = 0x0102'0304,
                                   .projectile_entity_id = 0x0506'0708},
        engine::network::SoundEvent{.source_entity_id = 0x0102'0304, .sound_id = 0x0506},
        engine::network::PlayerLeftEvent{.player_entity_id = 0x0102'0304},
        engine::network::MusicEvent{.music_id = 0x0506},
    };
}

inline engine::network::SnapshotPayload snapshot_with(std::size_t entity_count) {
    engine::network::SnapshotPayload snapshot{.server_tick = 0x0102'0304, .entities = {}};
    for (std::size_t index = 0; index < entity_count; ++index) {
        const auto value = static_cast<std::int16_t>(index);
        snapshot.entities.push_back(engine::network::EntityState{
            .entity_id = static_cast<std::uint32_t>(index + 1),
            .type_id = static_cast<std::uint16_t>(index % 3),
            .x = value,
            .y = static_cast<std::int16_t>(-value),
            .vx = static_cast<std::int16_t>(value * 2),
            .vy = static_cast<std::int16_t>(-value * 2),
        });
    }
    return snapshot;
}

inline engine::network::ConnectAcceptPayload
accept_with(std::vector<engine::network::TypeEntry> types) {
    return {
        .client_nonce = 0x0102'0304,
        .player_entity_id = 0x0506'0708,
        .types = std::move(types),
    };
}

inline std::vector<engine::network::TypeEntry> long_name_types(std::size_t count) {
    std::vector<engine::network::TypeEntry> types;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name(engine::network::max_type_name_length, 'a');
        name.back() = static_cast<char>('A' + index);
        types.push_back({.type_id = static_cast<std::uint16_t>(index), .name = std::move(name)});
    }
    return types;
}

} // namespace payload_samples
