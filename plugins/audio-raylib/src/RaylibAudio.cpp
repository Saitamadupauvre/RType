#include "RaylibAudio.hpp"

#include <cstddef>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace {

namespace log = engine::core::log;

constexpr std::size_t voices_per_sound = 4;

} // namespace

RaylibAudio::~RaylibAudio() { close(); }

bool RaylibAudio::open() {
    if (_open) {
        return true;
    }
    SetTraceLogLevel(LOG_WARNING);
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        log::warn("audio: no audio device available, sound is disabled");
        CloseAudioDevice();
        return false;
    }
    _open = true;
    return true;
}

void RaylibAudio::close() {
    if (!_open) {
        return;
    }
    stop_music();
    for (const LoadedSound& sound : _sounds.remove_all()) {
        free_sound(sound);
    }
    for (const Music& music : _musics.remove_all()) {
        UnloadMusicStream(music);
    }
    CloseAudioDevice();
    _open = false;
}

bool RaylibAudio::is_open() const { return _open; }

void RaylibAudio::update() {
    if (Music* music = playing_music()) {
        UpdateMusicStream(*music);
    }
}

engine::audio::SoundId RaylibAudio::load_sound(const std::filesystem::path& path) {
    if (!_open) {
        return {};
    }
    const Sound source = LoadSound(path.string().c_str());
    if (!IsSoundValid(source)) {
        log::warn("audio: cannot load sound {}", path.string());
        return {};
    }
    LoadedSound loaded{.voices = {source}, .cycle = VoiceCycle(voices_per_sound)};
    for (std::size_t alias = 1; alias < voices_per_sound; ++alias) {
        loaded.voices.push_back(LoadSoundAlias(source));
    }
    return _sounds.add(std::move(loaded));
}

void RaylibAudio::unload_sound(engine::audio::SoundId sound) {
    if (const auto removed = _sounds.remove(sound)) {
        free_sound(*removed);
    }
}

void RaylibAudio::play_sound(engine::audio::SoundId sound, float volume, float pitch) {
    LoadedSound* loaded = _sounds.find(sound);
    if (loaded == nullptr) {
        return;
    }
    const Sound& voice = loaded->voices[loaded->cycle.next()];
    SetSoundVolume(voice, _mixer.effect_volume(volume));
    SetSoundPitch(voice, clamp_pitch(pitch));
    PlaySound(voice);
}

engine::audio::MusicId RaylibAudio::load_music(const std::filesystem::path& path) {
    if (!_open) {
        return {};
    }
    const Music music = LoadMusicStream(path.string().c_str());
    if (!IsMusicValid(music)) {
        log::warn("audio: cannot load music {}", path.string());
        return {};
    }
    return _musics.add(music);
}

void RaylibAudio::unload_music(engine::audio::MusicId music) {
    if (_playing == music) {
        stop_music();
    }
    if (const auto removed = _musics.remove(music)) {
        UnloadMusicStream(*removed);
    }
}

void RaylibAudio::play_music(engine::audio::MusicId music, bool loop) {
    Music* next = _musics.find(music);
    if (next == nullptr) {
        return;
    }
    stop_music();
    next->looping = loop;
    SetMusicVolume(*next, _mixer.music_volume());
    PlayMusicStream(*next);
    _playing = music;
}

void RaylibAudio::stop_music() {
    if (Music* music = playing_music()) {
        StopMusicStream(*music);
    }
    _playing.reset();
}

void RaylibAudio::set_master_volume(float volume) {
    _mixer.set_master_volume(volume);
    if (Music* music = playing_music()) {
        SetMusicVolume(*music, _mixer.music_volume());
    }
}

void RaylibAudio::set_music_volume(float volume) {
    _mixer.set_music_volume(volume);
    if (Music* music = playing_music()) {
        SetMusicVolume(*music, _mixer.music_volume());
    }
}

void RaylibAudio::set_effects_volume(float volume) { _mixer.set_effects_volume(volume); }

void RaylibAudio::free_sound(const LoadedSound& sound) {
    for (std::size_t voice = 1; voice < sound.voices.size(); ++voice) {
        UnloadSoundAlias(sound.voices[voice]);
    }
    UnloadSound(sound.voices.front());
}

Music* RaylibAudio::playing_music() { return _playing ? _musics.find(*_playing) : nullptr; }
