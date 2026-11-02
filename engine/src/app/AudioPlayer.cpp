#include "AudioPlayer.hpp"

#include "engine/core/log/Log.hpp"

namespace engine::app {

namespace {

namespace log = core::log;

constexpr float sound_volume = 1.F;
constexpr float sound_pitch = 1.F;

} // namespace

AudioPlayer::AudioPlayer(audio::IAudio& speakers, const std::filesystem::path& assets)
    : _audio(speakers),
      _sounds(
          assets / "sounds",
          [&speakers](const std::filesystem::path& path) { return speakers.load_sound(path); },
          [&speakers](audio::SoundId sound) { speakers.unload_sound(sound); }),
      _musics(
          assets / "music",
          [&speakers](const std::filesystem::path& path) { return speakers.load_music(path); },
          [&speakers](audio::MusicId music) { speakers.unload_music(music); }) {}

void AudioPlayer::apply(const script::AudioSettings& settings) {
    if (_applied == settings) {
        return;
    }
    _audio.get().set_music_volume(settings.music_volume);
    _audio.get().set_effects_volume(settings.effects_volume);
    _audio.get().set_master_volume(settings.muted ? 0.F : 1.F);
    _applied = settings;
}

void AudioPlayer::play_sound(const network::StringTable& strings, std::uint16_t sound_id) {
    const auto name = name_of(strings, network::StringCategory::sound, sound_id);
    if (!name) {
        return;
    }
    const auto id = sound(*name);
    if (id != audio::SoundId{}) {
        _audio.get().play_sound(id, sound_volume, sound_pitch);
    }
}

void AudioPlayer::change_music(const network::StringTable& strings, std::uint16_t music_id) {
    if (music_id == network::no_music) {
        _audio.get().stop_music();
        return;
    }
    const auto name = name_of(strings, network::StringCategory::music, music_id);
    if (!name) {
        return;
    }
    const auto id = music(*name);
    if (id != audio::MusicId{}) {
        _audio.get().play_music(id, true);
    }
}

bool AudioPlayer::preload_sound(const std::string& name) {
    return !_audio.get().is_open() || sound(name) != audio::SoundId{};
}

bool AudioPlayer::preload_music(const std::string& name) {
    return !_audio.get().is_open() || music(name) != audio::MusicId{};
}

std::optional<std::string> AudioPlayer::name_of(const network::StringTable& strings,
                                                network::StringCategory category,
                                                std::uint16_t id) {
    const char* kind = category == network::StringCategory::sound ? "sound" : "music";
    const std::string* name = strings.name_of(category, id);
    if (name == nullptr) {
        log::warn("client: ignored unknown {} id {}", kind, id);
        return std::nullopt;
    }
    return *name;
}

audio::SoundId AudioPlayer::sound(const std::string& name) {
    return _audio.get().is_open() ? _sounds.get(name) : audio::SoundId{};
}

audio::MusicId AudioPlayer::music(const std::string& name) {
    return _audio.get().is_open() ? _musics.get(name) : audio::MusicId{};
}

} // namespace engine::app
