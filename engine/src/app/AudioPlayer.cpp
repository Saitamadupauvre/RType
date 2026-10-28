#include "AudioPlayer.hpp"

#include <algorithm>
#include <cctype>

#include "engine/core/log/Log.hpp"

namespace engine::app {

namespace {

namespace log = core::log;

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

AudioPlayer::AudioPlayer(audio::IAudio& audio, const std::filesystem::path& assets)
    : _audio(audio), _sound_folder(assets / "sounds"), _music_folder(assets / "music") {}

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

std::optional<std::string> AudioPlayer::name_of(const network::StringTable& strings,
                                                network::StringCategory category,
                                                std::uint16_t id) {
    const char* kind = category == network::StringCategory::sound ? "sound" : "music";
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

audio::SoundId AudioPlayer::sound(const std::string& name) {
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

audio::MusicId AudioPlayer::music(const std::string& name) {
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

} // namespace engine::app
