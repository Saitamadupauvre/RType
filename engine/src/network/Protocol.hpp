#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace engine::network {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::size_t max_datagram_size = 1200;
inline constexpr std::size_t encoded_packet_header_size = 13;
inline constexpr std::size_t max_payload_size = max_datagram_size - encoded_packet_header_size;

enum class MessageType : std::uint8_t {
    invalid = 0,
    connect_request = 1,
    connect_accept = 2,
    input = 3,
    snapshot = 4,
    event = 5,
    heartbeat = 6,
    disconnect = 7,
};

inline constexpr std::uint32_t protocol_magic = 0x5254'5950;
inline constexpr std::uint16_t protocol_version = 1;

struct InputPayload {
    std::uint8_t action_flags{};
    std::int8_t move_x{};
    std::int8_t move_y{};

    bool operator==(const InputPayload&) const = default;
};

struct ConnectRequestPayload {
    std::uint32_t client_nonce{};

    bool operator==(const ConnectRequestPayload&) const = default;
};

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

inline constexpr std::size_t snapshot_prefix_size = 6;
inline constexpr std::size_t entity_record_size = 14;
inline constexpr std::size_t max_snapshot_entities =
    (max_payload_size - snapshot_prefix_size) / entity_record_size;

struct PacketHeader {
    std::uint32_t magic{};
    std::uint16_t version{};
    MessageType type{};
    std::uint32_t sequence{};
    std::uint16_t payload_size{};
};

} // namespace engine::network
