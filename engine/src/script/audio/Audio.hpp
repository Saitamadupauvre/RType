#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "engine/script/AudioCommand.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

inline constexpr std::size_t max_audio_name_length = 63;
inline constexpr std::size_t max_queued_audio_commands = 64;

[[nodiscard]] bool is_valid_audio_name(std::string_view name) noexcept;

class Audio {
public:
    void allow_declarations(bool allowed) noexcept { _declarations_allowed = allowed; }

    void declare_sounds(sol::this_state state, const sol::object& names);
    void declare_musics(sol::this_state state, const sol::object& names);
    void play(sol::this_state state, const sol::object& name, const sol::object& source);
    void music(sol::this_state state, const sol::object& name);

    [[nodiscard]] std::vector<std::string> sound_names() const;
    [[nodiscard]] std::vector<std::string> music_names() const;
    [[nodiscard]] std::vector<AudioCommand> take_commands() noexcept;

private:
    using Names = std::set<std::string, std::less<>>;

    void declare(sol::this_state state, std::string_view label, Names& declared,
                 const sol::object& names) const;
    void queue(AudioCommand command);

    Names _sounds;
    Names _musics;
    std::vector<AudioCommand> _commands;
    bool _declarations_allowed{false};
    bool _overflow_reported{false};
};

} // namespace engine::script
