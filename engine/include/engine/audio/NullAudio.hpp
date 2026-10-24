#pragma once

#include <cstdint>
#include <filesystem>

#include "engine/audio/Export.hpp"
#include "engine/audio/IAudio.hpp"

namespace engine::audio {

/**
 * @brief Audio backend that plays nothing.
 *
 * Used when no audio plugin can be loaded or no device is available, so that the game keeps
 * running silently. It opens successfully and hands out a new id for every load, whatever the
 * file, so callers behave as with a real backend.
 *
 * @note Main thread only.
 */
class ENGINE_AUDIO_EXPORT NullAudio final : public IAudio {
public:
    NullAudio() = default;
    ~NullAudio() override;

    bool open() override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    void update() override;
    [[nodiscard]] SoundId load_sound(const std::filesystem::path& path) override;
    void unload_sound(SoundId sound) override;
    void play_sound(SoundId sound, float volume, float pitch) override;
    [[nodiscard]] MusicId load_music(const std::filesystem::path& path) override;
    void unload_music(MusicId music) override;
    void play_music(MusicId music, bool loop) override;
    void stop_music() override;
    void set_master_volume(float volume) override;
    void set_music_volume(float volume) override;
    void set_effects_volume(float volume) override;

private:
    std::uint32_t _next_id{1};
    bool _open{false};
};

} // namespace engine::audio
