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
 * @brief Published on the server's event bus when a session ends, and on the event bus of every
 * other client when it learns about it.
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
 * @name Game events
 *
 * On the server, the game publishes these on the event bus given to NetworkServer: each one is
 * sent to every connected player before the next snapshot, in the order published. On the
 * client, NetworkClient publishes them on its event bus when they arrive.
 *
 * Entity ids are the ids carried by snapshots: `engine::core::Entity::index()` on the server.
 *
 * @note Publish and receive them on the game thread. A client and a server must not share one
 * event bus, or the client's events are sent again.
 * @{
 */

/**
 * @brief An entity appeared. `type_id` must be in the server's type table.
 */
struct ENGINE_NETWORK_EXPORT EntitySpawned {
    std::uint32_t entity_id;
    std::uint16_t type_id;
    /** @brief Position in world units, rounded to whole units on the wire. */
    float x;
    float y;
};

/**
 * @brief An entity was removed from the world.
 */
struct ENGINE_NETWORK_EXPORT EntityDestroyed {
    std::uint32_t entity_id;
};

/**
 * @brief An entity fired a projectile.
 */
struct ENGINE_NETWORK_EXPORT ShotFired {
    std::uint32_t source_entity_id;
    std::uint32_t projectile_entity_id;
};

/**
 * @brief An entity played a sound, e.g. from the Lua `play(...)` function.
 */
struct ENGINE_NETWORK_EXPORT SoundPlayed {
    std::uint32_t source_entity_id;
    std::uint16_t sound_id;
};

/** @} */

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
