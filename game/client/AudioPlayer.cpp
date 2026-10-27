#include "AudioPlayer.hpp"

#include <algorithm>
#include <cctype>

#include "engine/core/log/Log.hpp"

namespace rtype::client {

namespace {

namespace log = engine::core::log;

constexpr float sound_volume = 1.F;
constexpr float sound_pitch = 1.F;

bool is_name_character(char character) noexcept {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
           character == '-' || character == '.';
}

} // namespace

bool is_plain_file_name(const std::string& name) noexcept {
    return !name.empty() && name.front() != '.' && std::ranges::all_of(name, is_name_character);
}

AudioPlayer::AudioPlayer(engine::audio::IAudio& audio, const std::filesystem::path& assets,
                         AudioSettings settings)
    : _audio(audio), _sound_folder(assets / "sounds"), _music_folder(assets / "music"),
      _muted(settings.muted) {
    audio.set_music_volume(settings.music_volume);
    audio.set_effects_volume(settings.effects_volume);
    audio.set_master_volume(_muted ? 0.F : 1.F);
}

void AudioPlayer::play_sound(const engine::network::StringTable& strings, std::uint16_t sound_id) {
    const auto name = name_of(strings, engine::network::StringCategory::sound, sound_id);
    if (!name) {
        return;
    }
    const auto id = sound(*name);
    if (id != engine::audio::SoundId{}) {
        _audio.get().play_sound(id, sound_volume, sound_pitch);
    }
}

void AudioPlayer::change_music(const engine::network::StringTable& strings,
                               std::uint16_t music_id) {
    if (music_id == engine::network::no_music) {
        _audio.get().stop_music();
        return;
    }
    const auto name = name_of(strings, engine::network::StringCategory::music, music_id);
    if (!name) {
        return;
    }
    const auto id = music(*name);
    if (id != engine::audio::MusicId{}) {
        _audio.get().play_music(id, true);
    }
}

void AudioPlayer::toggle_mute() {
    _muted = !_muted;
    _audio.get().set_master_volume(_muted ? 0.F : 1.F);
}

std::optional<std::string> AudioPlayer::name_of(const engine::network::StringTable& strings,
                                                engine::network::StringCategory category,
                                                std::uint16_t id) {
    const char* kind = category == engine::network::StringCategory::sound ? "sound" : "music";
    const std::string* name = strings.name_of(category, id);
    if (name == nullptr) {
        log::warn("client: ignored unknown {} id {}", kind, id);
        return std::nullopt;
    }
    if (!is_plain_file_name(*name)) {
        log::warn("client: ignored {} '{}': not a plain file name", kind, *name);
        return std::nullopt;
    }
    return *name;
}

engine::audio::SoundId AudioPlayer::sound(const std::string& name) {
    if (const auto loaded = _sounds.find(name); loaded != _sounds.end()) {
        return loaded->second;
    }
    if (!_audio.get().is_open()) {
        return {};
    }
    const auto id = _audio.get().load_sound(_sound_folder / name);
    _sounds.emplace(name, id);
    return id;
}

engine::audio::MusicId AudioPlayer::music(const std::string& name) {
    if (const auto loaded = _musics.find(name); loaded != _musics.end()) {
        return loaded->second;
    }
    if (!_audio.get().is_open()) {
        return {};
    }
    const auto id = _audio.get().load_music(_music_folder / name);
    _musics.emplace(name, id);
    return id;
}

} // namespace rtype::client
