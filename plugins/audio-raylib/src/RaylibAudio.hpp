#pragma once

#include <raylib.h>

#include <filesystem>
#include <optional>
#include <vector>

#include "AudioMixer.hpp"
#include "HandleTable.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/audio/Types.hpp"

class RaylibAudio final : public engine::audio::IAudio {
public:
    RaylibAudio() = default;
    ~RaylibAudio() override;

    bool open() override;
    void close() override;
    [[nodiscard]] bool is_open() const override;
    void update() override;
    [[nodiscard]] engine::audio::SoundId load_sound(const std::filesystem::path& path) override;
    void unload_sound(engine::audio::SoundId sound) override;
    void play_sound(engine::audio::SoundId sound, float volume, float pitch) override;
    [[nodiscard]] engine::audio::MusicId load_music(const std::filesystem::path& path) override;
    void unload_music(engine::audio::MusicId music) override;
    void play_music(engine::audio::MusicId music, bool loop) override;
    void stop_music() override;
    void set_master_volume(float volume) override;
    void set_music_volume(float volume) override;
    void set_effects_volume(float volume) override;

private:
    struct LoadedSound {
        std::vector<Sound> voices;
        VoiceCycle cycle;
    };

    static void free_sound(const LoadedSound& sound);
    [[nodiscard]] Music* playing_music();

    HandleTable<engine::audio::SoundId, LoadedSound> _sounds;
    HandleTable<engine::audio::MusicId, Music> _musics;
    std::optional<engine::audio::MusicId> _playing;
    AudioMixer _mixer;
    bool _open{false};
};
