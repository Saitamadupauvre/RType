#pragma once

#include <cstdint>
#include <string>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Marks an entity as sent to clients in every server snapshot.
 *
 * An entity is replicated when it has both `Replicated` and `engine::core::Position`. Its
 * velocity is always sent as zero.
 *
 * The color of its `engine::core::Sprite` is sent too when present, otherwise opaque white.
 *
 * @note The game registers this component on the server registry. `type_id` must be one of
 * the ids of `ServerOptions::types`, or clients drop the snapshot.
 */
struct ENGINE_NETWORK_EXPORT Replicated {
    std::uint16_t type_id;
    /** @brief Player id of the client controlling the entity, or `0` for none. */
    std::uint8_t owner = 0;
};

/**
 * @brief One entry of the type table the server sends in `connect_accept`.
 *
 * Clients drop every snapshot and spawn event that uses a type id missing from this table.
 *
 * @note Ids and names must be unique within a table, and names 1 to 63 bytes long.
 */
struct ENGINE_NETWORK_EXPORT ReplicatedType {
    std::uint16_t type_id;
    std::string name;
};

} // namespace engine::network
