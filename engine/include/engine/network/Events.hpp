#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/network/Audio.hpp"
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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief A game-defined event, such as an explosion, sent with named, typed arguments.
 *
 * On the server, `name` and every `Argument::key` must be in the server's string table
 * (categories `event` and `event_key`): an event with an unknown name or key, more than
 * `max_event_arguments` arguments, a repeated key, a list value, a non-finite float or a
 * string or asset longer than `max_event_text_length` bytes is dropped and logged. The server
 * checks no schema: what an event carries is up to the game.
 *
 * On the client, names and keys are the ones of the received string table. Vec2 values arrive
 * rounded to whole units and ints are clamped to 32 bits, as in snapshots.
 */
struct ENGINE_NETWORK_EXPORT NamedEvent {
    /** @brief One argument of the event. */
    struct Argument {
        std::string key;
        core::FieldValue value;

        bool operator==(const Argument&) const = default;
    };

    std::string name;
    std::vector<Argument> arguments;

    /**
     * @return The value of the argument @p key, or `nullptr` when the event has none. The
     * pointer stays valid until the event is changed or destroyed.
     */
    [[nodiscard]] const core::FieldValue* find(std::string_view key) const noexcept {
        const auto found = std::ranges::find(arguments, key, &Argument::key);
        return found == arguments.end() ? nullptr : &found->value;
    }

    bool operator==(const NamedEvent&) const = default;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/** @brief Most arguments a NamedEvent may carry. */
inline constexpr std::size_t max_event_arguments = 16;

/** @brief Longest string or asset value a NamedEvent argument may hold, in bytes. */
inline constexpr std::size_t max_event_text_length = 63;

/**
 * @brief A sound was played, e.g. from the Lua `play(...)` function.
 *
 * `sound_id` must be in the server's sound table. `source_entity_id` is `no_source_entity`
 * when no entity played it.
 */
struct ENGINE_NETWORK_EXPORT SoundPlayed {
    std::uint32_t source_entity_id;
    std::uint16_t sound_id;
};

/**
 * @brief The music changed, e.g. from the Lua `music(...)` function.
 *
 * `music_id` must be in the server's music table, or be `no_music` to stop the music. The music
 * loops until the next change.
 *
 * On the server it also becomes the current music sent in `connect_accept`, so players who join
 * later hear it. On the client it is published as well when the connection is accepted and the
 * server is playing a music.
 */
struct ENGINE_NETWORK_EXPORT MusicChanged {
    std::uint16_t music_id;
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
