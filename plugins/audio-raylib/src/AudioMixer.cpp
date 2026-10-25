#include "AudioMixer.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float min_pitch = 0.25F;
constexpr float max_pitch = 4.F;

} // namespace

float clamp_volume(float volume) noexcept {
    return std::isnan(volume) ? 0.F : std::clamp(volume, 0.F, 1.F);
}

float clamp_pitch(float pitch) noexcept {
    return std::isnan(pitch) ? 1.F : std::clamp(pitch, min_pitch, max_pitch);
}

float AudioMixer::effect_volume(float requested) const noexcept {
    return clamp_volume(requested) * _effects * _master;
}

float AudioMixer::music_volume() const noexcept { return _music * _master; }

std::size_t VoiceCycle::next() noexcept {
    if (_voices == 0) {
        return 0;
    }
    const std::size_t voice = _current;
    _current = (_current + 1) % _voices;
    return voice;
}
