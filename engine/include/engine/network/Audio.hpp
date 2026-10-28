#pragma once

#include <cstdint>

#include "engine/network/StringTable.hpp"

namespace engine::network {

/** @brief Music id meaning "no music": `MusicChanged` with it stops the music. */
inline constexpr std::uint16_t no_music = reserved_string_id;

/** @brief `SoundPlayed::source_entity_id` of a sound played by no entity. */
inline constexpr std::uint32_t no_source_entity = 0xFFFF'FFFF;

} // namespace engine::network
