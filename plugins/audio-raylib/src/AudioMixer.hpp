#pragma once

#include <cstddef>

[[nodiscard]] float clamp_volume(float volume) noexcept;
[[nodiscard]] float clamp_pitch(float pitch) noexcept;

class AudioMixer {
public:
    void set_master_volume(float volume) noexcept { _master = clamp_volume(volume); }
    void set_music_volume(float volume) noexcept { _music = clamp_volume(volume); }
    void set_effects_volume(float volume) noexcept { _effects = clamp_volume(volume); }

    [[nodiscard]] float effect_volume(float requested) const noexcept;
    [[nodiscard]] float music_volume() const noexcept;

private:
    float _master{1.F};
    float _music{1.F};
    float _effects{1.F};
};

class VoiceCycle {
public:
    explicit VoiceCycle(std::size_t voices) noexcept : _voices(voices) {}

    [[nodiscard]] std::size_t next() noexcept;

private:
    std::size_t _voices;
    std::size_t _current{0};
};
