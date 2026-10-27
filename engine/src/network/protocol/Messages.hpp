#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Events.hpp"
#include "engine/network/StringTable.hpp"

namespace engine::network {

struct ConnectRequestPayload {
    std::uint32_t client_nonce{};

    bool operator==(const ConnectRequestPayload&) const = default;
};

struct ConnectAcceptPayload {
    std::uint32_t client_nonce{};
    std::uint32_t player_entity_id{};
    StringTable strings{};
    std::uint16_t current_music{no_music};

    bool operator==(const ConnectAcceptPayload&) const = default;
};

struct InputPayload {
    std::uint8_t action_flags{};
    std::int8_t move_x{};
    std::int8_t move_y{};

    bool operator==(const InputPayload&) const = default;
};

struct FieldState {
    std::uint16_t field_id{};
    core::FieldValue value;

    bool operator==(const FieldState&) const = default;
};

struct EntityState {
    std::uint32_t entity_id{};
    std::uint16_t type_id{};
    std::vector<FieldState> fields;

    bool operator==(const EntityState&) const = default;
};

struct SnapshotPayload {
    std::uint32_t server_tick{};
    std::vector<EntityState> entities;

    bool operator==(const SnapshotPayload&) const = default;
};

enum class EventType : std::uint8_t {
    sound = 4,
    player_left = 5,
    music = 6,
    named = 7,
};

struct SoundEvent {
    std::uint32_t source_entity_id{};
    std::uint16_t sound_id{};

    bool operator==(const SoundEvent&) const = default;
};

struct PlayerLeftEvent {
    std::uint32_t player_entity_id{};

    bool operator==(const PlayerLeftEvent&) const = default;
};

struct MusicEvent {
    std::uint16_t music_id{};

    bool operator==(const MusicEvent&) const = default;
};

struct EventArgumentState {
    std::uint16_t key_id{};
    core::FieldValue value;

    bool operator==(const EventArgumentState&) const = default;
};

struct NamedEventState {
    std::uint16_t name_id{};
    std::vector<EventArgumentState> arguments;

    bool operator==(const NamedEventState&) const = default;
};

using EventPayload = std::variant<SoundEvent, PlayerLeftEvent, MusicEvent, NamedEventState>;

struct HeartbeatPayload {
    bool operator==(const HeartbeatPayload&) const = default;
};

struct DisconnectPayload {
    DisconnectReason reason{};

    bool operator==(const DisconnectPayload&) const = default;
};

using Message = std::variant<ConnectRequestPayload, ConnectAcceptPayload, InputPayload,
                             SnapshotPayload, EventPayload, HeartbeatPayload, DisconnectPayload>;

} // namespace engine::network
