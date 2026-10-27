#pragma once

#include <cstdint>

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
 * @note The game registers this component on the server registry. `type_id` must be an id of
 * category `type` in `ServerOptions::strings`, or clients drop the snapshot.
 */
struct ENGINE_NETWORK_EXPORT Replicated {
    std::uint16_t type_id;
    /** @brief Player id of the client controlling the entity, or `0` for none. */
    std::uint8_t owner = 0;
};

} // namespace engine::network
