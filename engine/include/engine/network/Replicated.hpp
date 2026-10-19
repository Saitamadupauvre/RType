#pragma once

#include <cstdint>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief Marks an entity as sent to clients in every server snapshot.
 *
 * An entity is replicated when it has both `Replicated` and `engine::core::Position`. Its
 * `engine::core::Velocity` is sent too when present, otherwise zero.
 *
 * @note The game registers this component on the server registry. `type_id` must be one of
 * the ids of the type table sent in `connect_accept`, or clients drop the snapshot.
 */
struct ENGINE_NETWORK_EXPORT Replicated {
    std::uint16_t type_id;
};

} // namespace engine::network
