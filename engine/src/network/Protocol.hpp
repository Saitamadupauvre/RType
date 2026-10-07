#pragma once

#include <cstddef>
#include <cstdint>

namespace engine::network {

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
};

struct PacketHeader {
    std::uint32_t magic{};
    std::uint16_t version{};
    MessageType type{};
    std::uint32_t sequence{};
    std::uint16_t payload_size{};
};

} // namespace engine::network

  // PLEASE REFER TO DOCS/PROTOCOL.MD FOR MORE INFO :D
