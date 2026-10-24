#include "Audio.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

#include "Declarations.hpp"
#include "Entities.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

bool is_name_character(char character) noexcept {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' ||
           character == '-' || character == '.';
}

std::string checked_name(sol::this_state state, std::string_view function,
                         const sol::object& name) {
    if (name.get_type() != sol::type::string) {
        throw std::runtime_error(
            std::format("{}{}: name must be a string", caller_location(state), function));
    }
    return name.as<std::string>();
}

std::optional<core::Entity> checked_source(sol::this_state state, const sol::object& source) {
    if (source.get_type() == sol::type::lua_nil || source.get_type() == sol::type::none) {
        return std::nullopt;
    }
    if (!source.is<EntityHandle>()) {
        throw std::runtime_error(
            std::format("{}play: source must be an entity or nil", caller_location(state)));
    }
    return source.as<EntityHandle>().entity;
}

} // namespace

bool is_valid_audio_name(std::string_view name) noexcept {
    return !name.empty() && name.size() <= max_audio_name_length && name.front() != '.' &&
           std::ranges::all_of(name, is_name_character);
}

void Audio::declare_sounds(sol::this_state state, const sol::object& names) {
    declare(state, "sounds", _sounds, names);
}

void Audio::declare_musics(sol::this_state state, const sol::object& names) {
    declare(state, "musics", _musics, names);
}

void Audio::declare(sol::this_state state, std::string_view label, Names& declared,
                    const sol::object& names) const {
    const std::string where = caller_location(state);
    if (!_declarations_allowed) {
        throw std::runtime_error(
            std::format("{}{} can only be called while the scripts load", where, label));
    }
    if (names.get_type() != sol::type::table) {
        throw std::runtime_error(std::format("{}{} expects a list of file names", where, label));
    }
    const auto list = names.as<sol::table>();
    Names added;
    for (std::size_t index = 1; index <= list.size(); ++index) {
        const sol::object entry = list[index];
        if (entry.get_type() != sol::type::string) {
            throw std::runtime_error(
                std::format("{}{}: entry {} must be a string", where, label, index));
        }
        auto name = entry.as<std::string>();
        if (!is_valid_audio_name(name)) {
            throw std::runtime_error(std::format(
                "{}{}: '{}' is not a valid file name: 1 to {} letters, digits, '_', '-' or '.', "
                "not starting with '.'",
                where, label, name, max_audio_name_length));
        }
        if (declared.contains(name) || !added.insert(std::move(name)).second) {
            throw std::runtime_error(std::format("{}{}: '{}' is already declared", where, label,
                                                 entry.as<std::string>()));
        }
    }
    declared.merge(added);
}

void Audio::play(sol::this_state state, const sol::object& name, const sol::object& source) {
    std::string sound = checked_name(state, "play", name);
    if (!_sounds.contains(sound)) {
        throw std::runtime_error(std::format("{}play: unknown sound '{}', declare it with sounds",
                                             caller_location(state), sound));
    }
    queue(AudioCommand{.kind = AudioCommandKind::Sound,
                       .name = std::move(sound),
                       .source = checked_source(state, source)});
}

void Audio::music(sol::this_state state, const sol::object& name) {
    if (name.get_type() == sol::type::lua_nil || name.get_type() == sol::type::none) {
        queue(
            AudioCommand{.kind = AudioCommandKind::StopMusic, .name = {}, .source = std::nullopt});
        return;
    }
    std::string track = checked_name(state, "music", name);
    if (!_musics.contains(track)) {
        throw std::runtime_error(std::format("{}music: unknown music '{}', declare it with musics",
                                             caller_location(state), track));
    }
    queue(AudioCommand{
        .kind = AudioCommandKind::Music, .name = std::move(track), .source = std::nullopt});
}

std::vector<std::string> Audio::sound_names() const { return {_sounds.begin(), _sounds.end()}; }

std::vector<std::string> Audio::music_names() const { return {_musics.begin(), _musics.end()}; }

std::vector<AudioCommand> Audio::take_commands() noexcept {
    _overflow_reported = false;
    return std::exchange(_commands, {});
}

void Audio::queue(AudioCommand command) {
    if (_commands.size() < max_queued_audio_commands) {
        _commands.push_back(std::move(command));
        return;
    }
    if (!_overflow_reported) {
        _overflow_reported = true;
        log::warn("lua: more than {} sounds and musics queued since the last tick, dropping the "
                  "rest",
                  max_queued_audio_commands);
    }
}

} // namespace engine::script
