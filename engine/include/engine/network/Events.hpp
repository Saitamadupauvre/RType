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

} // namespace engine::network
