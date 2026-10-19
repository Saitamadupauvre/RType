#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <vector>

#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Stage of a client's connection to the server.
 */
enum class ConnectionState : std::uint8_t {
    /** @brief Sending `connect_request` until the server answers. */
    connecting,
    /** @brief Accepted by the server: inputs are sent and snapshots received. */
    connected,
    /** @brief The server had no free player slot. */
    rejected,
    /** @brief The server never answered the connect requests. */
    gave_up,
    /** @brief The connection ended once accepted, was closed by the client, or never started. */
    disconnected,
};

/**
 * @brief One replicated entity, as last received from the server.
 */
struct ENGINE_NETWORK_EXPORT EntitySnapshot {
    std::uint32_t entity_id;
    /** @brief Type id given by the server to `Replicated::type_id`. */
    std::uint16_t type_id;
    /** @brief Position and velocity in world units, rounded to whole units by the protocol. */
    float x;
    float y;
    float vx;
    float vy;
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
 *    than the timeout.
 * 2. send_input(): sends the current input of the local player while connected.
 * 3. entities(): the newest world received from the server, for the game to draw.
 *
 * When the connection ends, for any reason, the network threads are stopped and `Disconnected`
 * is published once on the event bus. Packets arriving afterwards are ignored.
 *
 * @note Not thread-safe: call every member from the same thread. The clock and the event bus
 * must outlive the client. One client makes at most one connection.
 */
class ENGINE_NETWORK_EXPORT NetworkClient {
public:
    NetworkClient(const core::time::IClock& clock, core::event::EventBus& events);

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
     * @param action_flags Bit set of actions; only bit 0 (`fire`) is defined.
     * @param move_x Horizontal direction: `-1`, `0` or `1`.
     * @param move_y Vertical direction: `-1`, `0` or `1`.
     * @note An input outside these values is dropped and logged.
     */
    void send_input(std::uint8_t action_flags, std::int8_t move_x, std::int8_t move_y);

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
     * @return The server tick of the newest snapshot received, if any.
     */
    [[nodiscard]] std::optional<std::uint32_t> server_tick() const;

    /**
     * @return Every entity of the newest snapshot received. Kept after the connection ends.
     */
    [[nodiscard]] std::vector<EntitySnapshot> entities() const;

private:
    struct ImplNet;
    std::unique_ptr<ImplNet> impl_;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::network
