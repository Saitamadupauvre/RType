#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Stats.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/network/Transport.hpp"

namespace engine::network {

/**
 * @brief Settings of a NetworkServer, chosen by the game.
 */
struct ENGINE_NETWORK_EXPORT ServerOptions {
    /** @brief Number of player slots. A client arriving when all are taken is rejected. */
    std::size_t max_players;
    /** @brief Names sent to every client in `connect_accept`: every `Replicated::type_id`
     * (category `type`), `SoundPlayed::sound_id` (`sound`), `MusicChanged::music_id` (`music`),
     * `NamedEvent::name` (`event`) and `NamedEvent::Argument::key` (`event_key`) the game uses.
     * The server adds the replicated field names (`field`) itself. */
    StringTable strings = {};
};

/**
 * @brief Latest input received from one connected player.
 */
struct ENGINE_NETWORK_EXPORT PlayerInput {
    std::uint32_t player_id;
    /** @brief Keys and mouse buttons held, and the pointer with whole coordinates when the
     * client sent one. */
    core::InputState input;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Authoritative game server: player sessions, inputs and world snapshots.
 *
 * Packets travel through a ServerTransport (UDP or in-memory loopback); the server behaves the
 * same on both. Everything happens on the thread that calls receive() and send_snapshot(), once
 * per fixed tick:
 *
 * 1. receive(): handles every packet received since the last call (connections,
 *    disconnections, inputs) and removes players silent for longer than the session timeout.
 *    `PlayerJoined` and `PlayerLeft` are published on the event bus during this call.
 * 2. inputs(): the latest input of each connected player, for the game to apply.
 * 3. During the tick, the game publishes `SoundPlayed`, `MusicChanged` and `NamedEvent` on
 *    the event bus. Each one is queued for the players connected at that moment; a NamedEvent
 *    that breaks its rules is dropped and logged instead.
 * 4. send_snapshot(): sends the queued events in the order published, then every entity with
 *    `Replicated`, to every player.
 *
 * Malformed packets are dropped and logged; they never reach the game.
 *
 * @note Not thread-safe: call every member from the same thread. The clock, the event bus, the
 * field registry and the transport must outlive the server.
 */
class ENGINE_NETWORK_EXPORT NetworkServer {
public:
    /**
     * @param fields Fields of the server world. Every field flagged `replicated` is sent in
     * snapshots under the name `Component.field`, category `field` of the string table. Clients
     * must know every such name.
     * @param transport Carries the packets; opened by start() and closed by stop().
     */
    NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                  const core::FieldRegistry& fields, ServerTransport& transport,
                  ServerOptions options);
    ~NetworkServer();

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) = delete;
    NetworkServer& operator=(NetworkServer&&) = delete;

    /**
     * @brief Opens the transport: binds the UDP port and starts its threads, or joins the
     * loopback network.
     *
     * @param ec Set to `std::errc::invalid_argument` when the string table does not fit in one
     * packet, or when the replicated fields cannot be sent: a list field, a name longer than 63
     * characters, more than 255 fields, or an entity holding all of them that does not fit in
     * one snapshot. Set when the transport cannot open (for example a UDP port already in use);
     * cleared otherwise.
     * @return `true` when the server is listening.
     */
    bool start(std::error_code& ec);

    /**
     * @brief Ends every session, sends the messages still queued, then closes the transport.
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
     * @param world The server world. Only entities with `Replicated` are sent, with the
     * replicated fields they own. A string longer than 255 bytes or a non-finite float is left
     * out and logged. Ints are clamped to 32 bits and vec2 components rounded to 16-bit ints.
     */
    void send_snapshot(core::EntityManager& world);

    /**
     * @return Number of connected players.
     */
    [[nodiscard]] std::size_t player_count() const;

    /**
     * @return Traffic of every connected player. A summary is also logged every 10 seconds
     * during receive().
     */
    [[nodiscard]] std::vector<PlayerStats> stats() const;

private:
    struct ImplNet;
    std::unique_ptr<ImplNet> impl_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
