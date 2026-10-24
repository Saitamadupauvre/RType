#include "engine/audio/NullAudio.hpp"

namespace engine::audio {

NullAudio::~NullAudio() = default;

bool NullAudio::open() {
    _open = true;
    return true;
}

void NullAudio::close() { _open = false; }

bool NullAudio::is_open() const { return _open; }

void NullAudio::update() {}

SoundId NullAudio::load_sound(const std::filesystem::path&) {
    return _open ? SoundId{_next_id++} : SoundId{};
}

void NullAudio::unload_sound(SoundId) {}

void NullAudio::play_sound(SoundId, float, float) {}

MusicId NullAudio::load_music(const std::filesystem::path&) {
    return _open ? MusicId{_next_id++} : MusicId{};
}

void NullAudio::unload_music(MusicId) {}

void NullAudio::play_music(MusicId, bool) {}

void NullAudio::stop_music() {}

void NullAudio::set_master_volume(float) {}

void NullAudio::set_music_volume(float) {}

void NullAudio::set_effects_volume(float) {}

} // namespace engine::audio
