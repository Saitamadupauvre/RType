#pragma once

#include <filesystem>

#include "engine/audio/Export.hpp"
#include "engine/audio/Types.hpp"
#include "engine/core/NonCopyableNonMovable.hpp"

namespace engine::audio {

/**
 * @brief Sound effects and music provided by an audio backend.
 *
 * Effects are short sounds loaded in memory; several of them can play at once. Music is
 * streamed: at most one track plays at a time, and update() must be called every frame to keep
 * it playing.
 *
 * The volume heard is the volume given to the call, multiplied by the effects or music volume,
 * multiplied by the master volume. Every volume is clamped to [0, 1] and starts at 1.
 *
 * @note Main thread only. Unknown or default ids are ignored. Every call except open() and the
 * volume setters does nothing while the device is closed.
 */
class ENGINE_AUDIO_EXPORT IAudio : core::NonCopyableNonMovable {
public:
    virtual ~IAudio();

    /**
     * @brief Opens the audio device.
     *
     * @return true if the device is open, also when it already was; false when no device is
     * available, in which case the backend stays silent.
     */
    virtual bool open() = 0;

    /** @brief Stops every sound, unloads every sound and music, then closes the device. */
    virtual void close() = 0;

    /** @return true between a successful open() and close(). */
    [[nodiscard]] virtual bool is_open() const = 0;

    /** @brief Feeds the music stream. Call once per frame. */
    virtual void update() = 0;

    /**
     * @brief Loads a sound effect in memory.
     *
     * @param path Sound file, used as is; resolving it relative to the executable is the caller's
     * job.
     * @return The new sound, or the default id when the device is closed or the file cannot be
     * read or decoded.
     */
    [[nodiscard]] virtual SoundId load_sound(const std::filesystem::path& path) = 0;

    /** @brief Stops and frees a sound. Its id is never handed out again. */
    virtual void unload_sound(SoundId sound) = 0;

    /**
     * @brief Plays a sound effect, on top of the ones already playing.
     *
     * @param sound Sound returned by load_sound().
     * @param volume Volume of this play, clamped to [0, 1].
     * @param pitch Playback speed factor, clamped to [0.25, 4]: 1 keeps the sound unchanged.
     */
    virtual void play_sound(SoundId sound, float volume, float pitch) = 0;

    /**
     * @brief Opens a music file for streaming.
     *
     * @param path Music file, used as is.
     * @return The new music, or the default id when the device is closed or the file cannot be
     * opened.
     */
    [[nodiscard]] virtual MusicId load_music(const std::filesystem::path& path) = 0;

    /** @brief Stops and frees a music. Its id is never handed out again. */
    virtual void unload_music(MusicId music) = 0;

    /**
     * @brief Plays a music from its start, stopping the music playing before.
     *
     * @param music Music returned by load_music().
     * @param loop Whether the music starts again when it ends.
     */
    virtual void play_music(MusicId music, bool loop) = 0;

    /** @brief Stops the music playing, if any. */
    virtual void stop_music() = 0;

    /** @brief Sets the volume applied to effects and music, clamped to [0, 1]. */
    virtual void set_master_volume(float volume) = 0;

    /** @brief Sets the volume applied to music, clamped to [0, 1]. Applies to the music playing. */
    virtual void set_music_volume(float volume) = 0;

    /** @brief Sets the volume applied to effects played from now on, clamped to [0, 1]. */
    virtual void set_effects_volume(float volume) = 0;
};

} // namespace engine::audio
