#pragma once

#include <cstdint>

namespace engine::audio {

/**
 * @brief Handle to a sound effect owned by an audio backend.
 *
 * @note The default value refers to no sound: playing it does nothing.
 */
struct SoundId {
    std::uint32_t value{0};

    bool operator==(const SoundId&) const = default;
};

/**
 * @brief Handle to a music track owned by an audio backend.
 *
 * @note The default value refers to no music: playing it does nothing.
 */
struct MusicId {
    std::uint32_t value{0};

    bool operator==(const MusicId&) const = default;
};

} // namespace engine::audio
