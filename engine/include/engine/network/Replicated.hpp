#pragma once

#include <cstdint>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Marks an entity as sent to clients in every server snapshot.
 *
 * A snapshot entity carries its `type_id` and every field flagged
 * core::FieldFlags::replicated that the entity owns, read through the core::FieldRegistry given
 * to NetworkServer. Fields of components the entity lacks are left out.
 *
 * @note The game registers this component on the server world. `type_id` must be an id of
 * category `type` in `ServerOptions::strings`, or clients drop the snapshot.
 * @see register_network_fields
 */
struct ENGINE_NETWORK_EXPORT Replicated {
    std::uint16_t type_id;
    /** @brief Player id of the client controlling the entity, or `0` for none. */
    std::uint8_t owner = 0;
};

} // namespace engine::network
