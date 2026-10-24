#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/audio/IAudio.hpp"
#include "engine/audio/Types.hpp"
#include "engine/network/Audio.hpp"

namespace rtype::client {

struct AudioSettings {
    float music_volume{1.F};
    float effects_volume{1.F};
    bool muted{false};
};

class AudioPlayer {
public:
    AudioPlayer(engine::audio::IAudio& audio, const std::filesystem::path& assets,
                AudioSettings settings);

    void play_sound(const std::vector<engine::network::AudioName>& table, std::uint16_t sound_id);
    void change_music(const std::vector<engine::network::AudioName>& table, std::uint16_t music_id);
    void toggle_mute();
    [[nodiscard]] bool muted() const noexcept { return _muted; }

private:
    [[nodiscard]] static std::optional<std::string>
    name_of(const std::vector<engine::network::AudioName>& table, std::uint16_t id,
            const char* kind);
    engine::audio::SoundId sound(const std::string& name);
    engine::audio::MusicId music(const std::string& name);

    std::reference_wrapper<engine::audio::IAudio> _audio;
    std::filesystem::path _sound_folder;
    std::filesystem::path _music_folder;
    std::unordered_map<std::string, engine::audio::SoundId> _sounds;
    std::unordered_map<std::string, engine::audio::MusicId> _musics;
    bool _muted;
};

[[nodiscard]] bool is_plain_file_name(const std::string& name) noexcept;

} // namespace rtype::client
