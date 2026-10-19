#pragma once

#include <cstdint>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Published on the server's event bus when a client is accepted into a session.
 *
 * @note Published on the game thread, once per accepted client. A repeated connect request
 * from the same endpoint does not publish it again.
 */
struct ENGINE_NETWORK_EXPORT PlayerJoined {
    std::uint32_t player_id;
};

/**
 * @brief Published on the server's event bus when a session ends.
 *
 * Fired when the client sends a disconnect or stops sending packets for longer than the
 * session timeout. The player id becomes free for the next client.
 *
 * @note Published on the game thread.
 */
struct ENGINE_NETWORK_EXPORT PlayerLeft {
    std::uint32_t player_id;
};

/**
 * @brief Why a connection ended. The values are the ones carried by a `disconnect` packet.
 */
enum class DisconnectReason : std::uint8_t {
    /** @brief The peer closed the connection on purpose. */
    normal = 0,
    /** @brief No packet arrived from the peer before the timeout. */
    timeout = 1,
    /** @brief The server is stopping. */
    server_shutdown = 2,
    /** @brief The peer broke the protocol. */
    protocol_error = 3,
    /** @brief Every player slot of the server is taken. */
    server_full = 4,
};

/**
 * @brief Published on the client's event bus when its connection to the server ends.
 *
 * Published once per connection, whatever ended it:
 * - the reason sent by the server in a `disconnect` packet;
 * - `server_full` when the server rejected the connection;
 * - `timeout` when the server never answered, or stopped sending packets once connected;
 * - `normal` after NetworkClient::disconnect().
 *
 * @note Published on the game thread, from NetworkClient::update() or
 * NetworkClient::disconnect().
 */
struct ENGINE_NETWORK_EXPORT Disconnected {
    DisconnectReason reason;
};

} // namespace engine::network
