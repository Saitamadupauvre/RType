#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace engine::network {

struct ConnectRequestPayload {
    std::uint32_t client_nonce{};

    bool operator==(const ConnectRequestPayload&) const = default;
};

struct TypeEntry {
    std::uint16_t type_id{};
    std::string name;

    bool operator==(const TypeEntry&) const = default;
};

struct ConnectAcceptPayload {
    std::uint32_t client_nonce{};
    std::uint32_t player_entity_id{};
    std::vector<TypeEntry> types;

    bool operator==(const ConnectAcceptPayload&) const = default;
};

struct InputPayload {
    std::uint8_t action_flags{};
    std::int8_t move_x{};
    std::int8_t move_y{};

    bool operator==(const InputPayload&) const = default;
};

struct EntityState {
    std::uint32_t entity_id{};
    std::uint16_t type_id{};
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t vx{};
    std::int16_t vy{};

    bool operator==(const EntityState&) const = default;
};

struct SnapshotPayload {
    std::uint32_t server_tick{};
    std::vector<EntityState> entities;

    bool operator==(const SnapshotPayload&) const = default;
};

enum class EventType : std::uint8_t {
    spawn = 1,
    destroy = 2,
    fire = 3,
    sound = 4,
};

struct SpawnEvent {
    std::uint32_t entity_id{};
    std::uint16_t type_id{};
    std::int16_t x{};
    std::int16_t y{};

    bool operator==(const SpawnEvent&) const = default;
};

struct DestroyEvent {
    std::uint32_t entity_id{};

    bool operator==(const DestroyEvent&) const = default;
};

struct FireEvent {
    std::uint32_t source_entity_id{};
    std::uint32_t projectile_entity_id{};

    bool operator==(const FireEvent&) const = default;
};

struct SoundEvent {
    std::uint32_t source_entity_id{};
    std::uint16_t sound_id{};

    bool operator==(const SoundEvent&) const = default;
};

using EventPayload = std::variant<SpawnEvent, DestroyEvent, FireEvent, SoundEvent>;

struct HeartbeatPayload {
    bool operator==(const HeartbeatPayload&) const = default;
};

enum class DisconnectReason : std::uint8_t {
    normal = 0,
    timeout = 1,
    server_shutdown = 2,
    protocol_error = 3,
};

struct DisconnectPayload {
    DisconnectReason reason{};

    bool operator==(const DisconnectPayload&) const = default;
};

using Message = std::variant<ConnectRequestPayload, ConnectAcceptPayload, InputPayload,
                             SnapshotPayload, EventPayload, HeartbeatPayload, DisconnectPayload>;

} // namespace engine::network
