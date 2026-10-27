#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/network/StringTable.hpp"

#include "encoder/PayloadEncoder.hpp"
#include "protocol/Protocol.hpp"

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

inline constexpr std::uint16_t position_field = 1;
inline constexpr std::uint16_t velocity_field = 2;
inline constexpr std::size_t moving_entity_size = 21;

inline engine::network::EntityState moving_entity(std::uint32_t entity_id, std::uint16_t type_id,
                                                  float x, float y) {
    return {.entity_id = entity_id,
            .type_id = type_id,
            .fields = {{.field_id = position_field, .value = engine::core::Vec2{x, y}},
                       {.field_id = velocity_field, .value = engine::core::Vec2{2 * x, -2 * y}}}};
}

inline engine::network::SnapshotPayload snapshot_with(std::size_t entity_count) {
    engine::network::SnapshotPayload snapshot{.server_tick = 0x0102'0304, .entities = {}};
    for (std::size_t index = 0; index < entity_count; ++index) {
        const auto value = static_cast<float>(index);
        snapshot.entities.push_back(moving_entity(static_cast<std::uint32_t>(index + 1),
                                                  static_cast<std::uint16_t>(index % 3), value,
                                                  -value));
    }
    return snapshot;
}

inline constexpr std::size_t max_moving_entities =
    engine::network::max_snapshot_entity_bytes / moving_entity_size;

inline engine::network::StringTable strings_of(std::vector<engine::network::StringEntry> entries) {
    engine::network::StringTable table;
    for (auto& entry : entries) {
        EXPECT_TRUE(table.insert(std::move(entry)));
    }
    return table;
}

inline engine::network::ConnectAcceptPayload accept_with(engine::network::StringTable strings) {
    return {
        .client_nonce = 0x0102'0304,
        .player_entity_id = 0x0506'0708,
        .strings = std::move(strings),
    };
}

inline engine::network::StringTable long_name_strings(std::size_t count) {
    engine::network::StringTable table;
    for (std::size_t index = 0; index < count; ++index) {
        std::string name(engine::network::max_string_name_length, 'a');
        name.back() = static_cast<char>('A' + index);
        EXPECT_TRUE(table.insert({.category = engine::network::StringCategory::type,
                                  .id = static_cast<std::uint16_t>(index),
                                  .name = std::move(name)}));
    }
    return table;
}

} // namespace payload_samples
