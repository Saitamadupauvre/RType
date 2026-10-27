#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine::network {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::uint32_t protocol_magic = 0x5254'5950;
inline constexpr std::uint16_t protocol_version = 6;

inline constexpr std::size_t max_datagram_size = 1200;
inline constexpr std::size_t encoded_packet_header_size = 13;
inline constexpr std::size_t max_payload_size = max_datagram_size - encoded_packet_header_size;

inline constexpr std::size_t snapshot_prefix_size = 6;
inline constexpr std::size_t max_snapshot_entity_bytes = max_payload_size - snapshot_prefix_size;
inline constexpr std::size_t entity_header_size = 7;
inline constexpr std::size_t field_header_size = 3;
inline constexpr std::size_t max_entity_fields = 255;
inline constexpr std::size_t max_wire_text_length = 255;

inline constexpr std::size_t connect_accept_prefix_size = 12;
inline constexpr std::size_t min_string_entry_size = 4;

inline constexpr std::size_t named_event_header_size = 4;
inline constexpr std::size_t event_argument_header_size = 3;

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

struct PacketHeader {
    std::uint32_t magic{};
    std::uint16_t version{};
    MessageType type{};
    std::uint32_t sequence{};
    std::uint16_t payload_size{};
};

} // namespace engine::network
