#pragma once

#include <cstdint>
#include <string>

#include "engine/network/Export.hpp"

namespace engine::network {

/**
 * @brief One entry of the sound or music table the server sends in `connect_accept`.
 *
 * The name is what clients load, for example a file name under their sound folder.
 *
 * @note Ids and names must be unique within a table, and names 1 to 63 bytes long. Id
 * `no_music` is reserved in the music table.
 */
struct ENGINE_NETWORK_EXPORT AudioName {
    std::uint16_t id;
    std::string name;
};

/** @brief Music id meaning "no music": `MusicChanged` with it stops the music. */
inline constexpr std::uint16_t no_music = 0xFFFF;

/** @brief `SoundPlayed::source_entity_id` of a sound played by no entity. */
inline constexpr std::uint32_t no_source_entity = 0xFFFF'FFFF;

} // namespace engine::network
