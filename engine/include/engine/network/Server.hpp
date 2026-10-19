#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Replicated.hpp"

namespace engine::network {

/**
 * @brief Settings of a NetworkServer, chosen by the game.
 */
struct ENGINE_NETWORK_EXPORT ServerOptions {
    /** @brief UDP port to listen on, on every IPv4 interface. `0` picks a free port. */
    std::uint16_t port;
    /** @brief Number of player slots. A client arriving when all are taken is rejected. */
    std::size_t max_players;
    /** @brief Type table sent to every client in `connect_accept`: every `Replicated::type_id`
     * and `EntitySpawned::type_id` the game uses. */
    std::vector<ReplicatedType> types = {};
};

/**
 * @brief Latest input received from one connected player.
 */
struct ENGINE_NETWORK_EXPORT PlayerInput {
    std::uint32_t player_id;
    std::uint8_t action_flags;
    std::int8_t move_x;
    std::int8_t move_y;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Authoritative game server: player sessions, inputs and world snapshots over UDP.
 *
 * The socket is read and written by two background threads. Everything else happens on the
 * thread that calls receive() and send_snapshot(), once per fixed tick:
 *
 * 1. receive(): handles every packet received since the last call (connections,
 *    disconnections, inputs) and removes players silent for longer than the session timeout.
 *    `PlayerJoined` and `PlayerLeft` are published on the event bus during this call.
 * 2. inputs(): the latest input of each connected player, for the game to apply.
 * 3. During the tick, the game publishes `EntitySpawned`, `EntityDestroyed`, `ShotFired` and
 *    `SoundPlayed` on the event bus. Each one is queued for the players connected at that
 *    moment.
 * 4. send_snapshot(): sends the queued events in the order published, then every entity with
 *    `Replicated` and `Position`, to every player.
 *
 * Malformed packets are dropped and logged; they never reach the game.
 *
 * @note Not thread-safe: call every member from the same thread. The clock and the event bus
 * must outlive the server.
 */
class ENGINE_NETWORK_EXPORT NetworkServer {
public:
    NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                  ServerOptions options);
    ~NetworkServer();

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) = delete;
    NetworkServer& operator=(NetworkServer&&) = delete;

    /**
     * @brief Binds the UDP port and starts the network threads.
     *
     * @param ec Set to `std::errc::invalid_argument` when `ServerOptions::types` has a
     * duplicate id or name, or a name that is empty or longer than 63 bytes; set when the port
     * cannot be bound (for example already in use); cleared otherwise.
     * @return `true` when the server is listening.
     */
    bool start(std::error_code& ec);

    /**
     * @brief Ends every session, sends the messages still queued, then closes the socket and
     * joins the threads.
     *
     * Every connected player receives `disconnect` with reason `server_shutdown`. `PlayerLeft`
     * is not published for them.
     *
     * @note Idempotent; the destructor calls it.
     */
    void stop();

    /**
     * @brief Handles every packet received since the last call and drops timed-out players.
     */
    void receive();

    /**
     * @return The latest input of every connected player, idle until a first input arrives.
     */
    [[nodiscard]] std::vector<PlayerInput> inputs() const;

    /**
     * @brief Advances the server tick and sends a snapshot of the registry to every player.
     *
     * The game events queued since the last call are sent first.
     *
     * @param registry The server world. Only entities with `Replicated` and `Position` are sent.
     */
    void send_snapshot(core::EntityManager& registry);

    /**
     * @return The bound port once started (useful with port `0`), otherwise the requested one.
     */
    [[nodiscard]] std::uint16_t port() const;

    /**
     * @return Number of connected players.
     */
    [[nodiscard]] std::size_t player_count() const;

private:
    struct ImplNet;
    std::unique_ptr<ImplNet> impl_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
