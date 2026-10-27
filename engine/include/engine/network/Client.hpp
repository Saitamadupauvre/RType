#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "engine/core/Components.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/Export.hpp"
#include "engine/network/Replicated.hpp"
#include "engine/network/Stats.hpp"
#include "engine/network/StringTable.hpp"

namespace engine::network {

/**
 * @brief Stage of a client's connection to the server.
 */
enum class ConnectionState : std::uint8_t {
    /** @brief Sending `connect_request` until the server answers. */
    connecting,
    /** @brief Accepted by the server: inputs are sent and snapshots received. */
    connected,
    /** @brief The server had no free player slot, or replicates a field this client does not
     * know. */
    rejected,
    /** @brief The server never answered the connect requests. */
    gave_up,
    /** @brief The connection ended once accepted, was closed by the client, or never started. */
    disconnected,
};

/**
 * @brief One replicated field of an entity, as last received from the server.
 */
struct ENGINE_NETWORK_EXPORT ReplicatedValue {
    /** @brief The field in the registry given to NetworkClient. */
    core::FieldRef field;
    /** @brief A value of the field's kind. Vec2 components are whole numbers. */
    core::FieldValue value;
};

/**
 * @brief One replicated entity, as last received from the server.
 */
struct ENGINE_NETWORK_EXPORT EntitySnapshot {
    std::uint32_t entity_id;
    /** @brief Type id given by the server to `Replicated::type_id`. */
    std::uint16_t type_id;
    /** @brief The replicated fields the entity owns on the server, such as `Position.pos`. */
    std::vector<ReplicatedValue> fields;

    /** @return The value of field @p field of component @p component, or nullptr if absent. */
    [[nodiscard]] const core::FieldValue* find(std::string_view component,
                                               std::string_view field) const;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Client side of the game protocol: one connection to a server over UDP.
 *
 * The socket is read and written by two background threads. Everything else happens on the
 * thread that calls update(), once per frame:
 *
 * 1. update(): handles every packet received since the last call, resends the connect request
 *    or a heartbeat when due, and ends the connection when the server stays silent for longer
 *    than the timeout. Game events from the server (`SoundPlayed`, `MusicChanged`,
 *    `NamedEvent`, `PlayerLeft`) are published on the event bus during this call, in the order
 *    received; a NamedEvent naming an id missing from the string table is dropped and logged. When
 * the connection is accepted while the server plays a music, `MusicChanged` is published with it.
 * 2. send_input(): sends the current input of the local player while connected.
 * 3. entities(): the newest world received from the server, for the game to draw.
 *
 * When the connection ends, for any reason, the network threads are stopped and `Disconnected`
 * is published once on the event bus. Packets arriving afterwards are ignored.
 *
 * Snapshots and spawn events are checked against the type table the server sent in
 * `connect_accept`: one with a type id missing from the table is dropped whole and logged.
 *
 * Every field name the server replicates must be known to the client's field registry, or the
 * connection ends as `rejected` right after `connect_accept`. Snapshot fields with an unknown id
 * or a kind other than the registry's drop the whole snapshot.
 *
 * @note Not thread-safe: call every member from the same thread. The clock, the event bus and
 * the field registry must outlive the client. One client makes at most one connection.
 */
class ENGINE_NETWORK_EXPORT NetworkClient {
public:
    /** @param fields Fields the server may replicate, matched by `Component.field` name. */
    NetworkClient(const core::time::IClock& clock, core::event::EventBus& events,
                  const core::FieldRegistry& fields);

    /**
     * @brief Closes the connection like disconnect(), without publishing `Disconnected`.
     */
    ~NetworkClient();

    NetworkClient(const NetworkClient&) = delete;
    NetworkClient& operator=(const NetworkClient&) = delete;
    NetworkClient(NetworkClient&&) = delete;
    NetworkClient& operator=(NetworkClient&&) = delete;

    /**
     * @brief Opens a socket on a free local port and starts connecting to the server.
     *
     * The first `connect_request` is sent by the next update().
     *
     * @param server_ip Numeric IPv4 or IPv6 address of the server, e.g. `127.0.0.1`.
     * @param port UDP port of the server.
     * @param ec Set to `std::errc::invalid_argument` when `server_ip` is not a numeric address,
     * to `std::errc::already_connected` on a second call, or to the socket error; cleared
     * otherwise.
     * @return `true` when the client is connecting.
     */
    bool connect(std::string_view server_ip, std::uint16_t port, std::error_code& ec);

    /**
     * @brief Handles every packet received since the last call, then sends what is due.
     *
     * Does nothing before connect() or once the connection has ended.
     */
    void update();

    /**
     * @brief Sends the current input of the local player. Does nothing unless connected.
     *
     * Every key and mouse button held is sent; the server's scripts decide what they mean. The
     * pointer is rounded to whole virtual units and clamped to 16-bit ints; a NaN coordinate is
     * sent as 0.
     *
     * @param input Keys and mouse buttons held, and the optional pointer position.
     */
    void send_input(const core::InputState& input);

    /**
     * @brief Tells the server the player leaves, then stops the network threads.
     *
     * Publishes `Disconnected` with reason `normal` if the connection was still open.
     * @note Idempotent.
     */
    void disconnect();

    /**
     * @return The stage of the connection; `disconnected` before connect().
     */
    [[nodiscard]] ConnectionState state() const;

    /**
     * @return The player id given by the server, or `0` until accepted.
     */
    [[nodiscard]] std::uint32_t player_id() const;

    /**
     * @return The names the server sent in `connect_accept`, to name the
     * `EntitySnapshot::type_id` of each entity (category `type`), `SoundPlayed::sound_id`
     * (`sound`) and `MusicChanged::music_id` (`music`); NamedEvent already carries its names.
     * Empty until accepted. Kept after the
     * connection ends.
     */
    [[nodiscard]] StringTable strings() const;

    /**
     * @return The server tick of the newest snapshot received, if any.
     */
    [[nodiscard]] std::optional<std::uint32_t> server_tick() const;

    /**
     * @return Every entity of the newest snapshot received. Kept after the connection ends.
     */
    [[nodiscard]] std::vector<EntitySnapshot> entities() const;

    /**
     * @return Traffic exchanged with the server, round-trip time included; all zero before
     * connect(). A summary is also logged every 10 seconds during update().
     */
    [[nodiscard]] NetworkStats stats() const;

private:
    struct ImplNet;
    std::unique_ptr<ImplNet> impl_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
