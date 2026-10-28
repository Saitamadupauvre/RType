#pragma once

#include <cstdint>

#include "protocol/Messages.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"

namespace engine::network {

enum class PeerId : std::uint32_t {};

inline constexpr PeerId server_peer{0};

struct Inbound {
    PeerId from{};
    ReceivedMessage message;
};

struct Outbound {
    PeerId to{};
    Message message;
    std::uint32_t sequence{};
};

struct AddressedInbound {
    Endpoint from;
    ReceivedMessage message;
};

struct AddressedOutbound {
    Endpoint to;
    Message message;
    std::uint32_t sequence{};
};

} // namespace engine::network
