#pragma once

#include <cstdint>
#include <optional>

#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Traffic measured on one connection: one player on the server, or the server on a
 * client.
 *
 * Rates count the packets and bytes of the last complete second of the engine clock; they
 * are zero once a full second passes without traffic. Bytes are whole datagrams, header
 * included.
 */
struct ENGINE_NETWORK_EXPORT NetworkStats {
    std::uint64_t packets_sent_per_second{};
    std::uint64_t packets_received_per_second{};
    std::uint64_t bytes_sent_per_second{};
    std::uint64_t bytes_received_per_second{};
    /** @brief Packets from the peer never received, from gaps in its sequence numbers, since the
     * connection started. A packet arriving late is no longer counted lost. */
    std::uint64_t packets_lost{};
    /** @brief Packets from the peer that arrived after a newer one, since the connection
     * started. */
    std::uint64_t packets_reordered{};
    /** @brief Time between the latest heartbeat and the server's answer. Measured by clients
     * only; empty on the server and before the first answer. */
    std::optional<core::time::Duration> round_trip_time;
};

/**
 * @brief Traffic of one connected player, as seen by the server.
 */
struct ENGINE_NETWORK_EXPORT PlayerStats {
    std::uint32_t player_id{};
    NetworkStats network;
};

} // namespace engine::network
