#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

#include "engine/app/Export.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/audio/Types.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/script/ClientApi.hpp"

namespace engine::app {

class ENGINE_APP_EXPORT AudioPlayer {
public:
    AudioPlayer(audio::IAudio& audio, const std::filesystem::path& assets);

    void play_sound(const network::StringTable& strings, std::uint16_t sound_id);
    void change_music(const network::StringTable& strings, std::uint16_t music_id);
    void apply(const script::AudioSettings& settings);

private:
    [[nodiscard]] static std::optional<std::string> name_of(const network::StringTable& strings,
                                                            network::StringCategory category,
                                                            std::uint16_t id);
    audio::SoundId sound(const std::string& name);
    audio::MusicId music(const std::string& name);

    std::reference_wrapper<audio::IAudio> _audio;
    std::filesystem::path _sound_folder;
    std::filesystem::path _music_folder;
    std::unordered_map<std::string, audio::SoundId> _sounds;
    std::unordered_map<std::string, audio::MusicId> _musics;
    std::optional<script::AudioSettings> _applied;
};

[[nodiscard]] ENGINE_APP_EXPORT bool is_plain_file_name(const std::string& name) noexcept;

} // namespace engine::app
