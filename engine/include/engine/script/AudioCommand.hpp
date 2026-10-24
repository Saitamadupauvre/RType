#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "engine/core/Entity.hpp"

namespace engine::script {

/** @brief What a script asked for with `play(...)` or `music(...)`. */
enum class AudioCommandKind : std::uint8_t {
    /** @brief `play(name)`: play the sound effect `name`. */
    Sound,
    /** @brief `music(name)`: play the music `name` in a loop, replacing the current one. */
    Music,
    /** @brief `music(nil)`: stop the music. */
    StopMusic,
};

/**
 * @brief One audio request queued by a script, for the host to play or send to clients.
 */
struct AudioCommand {
    AudioCommandKind kind;
    /** @brief Name declared with `sounds { ... }` or `musics { ... }`; empty for StopMusic. */
    std::string name;
    /** @brief Entity given as second argument of `play`, if any. */
    std::optional<core::Entity> source;

    /** @brief Two commands are equal when every field is equal. */
    bool operator==(const AudioCommand&) const = default;
};

} // namespace engine::script
