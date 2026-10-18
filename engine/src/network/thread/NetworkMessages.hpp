#pragma once

#include "protocol/Messages.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"

namespace engine::network {

struct Inbound {
    Endpoint from;
    ReceivedMessage message;
};

struct Outbound {
    Endpoint to;
    Message message;
};

} // namespace engine::network
