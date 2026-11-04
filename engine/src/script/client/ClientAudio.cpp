#include "client/ClientAudio.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "lua/LuaErrors.hpp"

namespace engine::script {

void ClientAudio::bind(sol::state& lua) {
    sol::table table = lua.create_named_table("audio");
    table.set_function("muted", [this] { return _settings.muted; });
    table.set_function("set_muted", [this](sol::this_state state, const sol::object& muted) {
        set_muted(state, muted);
    });
    table.set_function("volume", [this](sol::this_state state, const sol::object& kind) {
        return volume(state, kind, "volume");
    });
    table.set_function("set_volume",
                       [this](sol::this_state state, const sol::object& kind,
                              const sol::object& value) { set_volume(state, kind, value); });
}

void ClientAudio::set_settings(AudioSettings settings) noexcept {
    settings.music_volume = std::clamp(settings.music_volume, 0.F, 1.F);
    settings.effects_volume = std::clamp(settings.effects_volume, 0.F, 1.F);
    _settings = settings;
}

float& ClientAudio::volume(sol::this_state state, const sol::object& kind,
                           std::string_view function) {
    if (kind.get_type() == sol::type::string) {
        const auto name = kind.as<std::string_view>();
        if (name == "music") {
            return _settings.music_volume;
        }
        if (name == "effects") {
            return _settings.effects_volume;
        }
    }
    throw std::runtime_error(std::format(R"({}audio.{}: kind must be "music" or "effects")",
                                         caller_location(state), function));
}

void ClientAudio::set_volume(sol::this_state state, const sol::object& kind,
                             const sol::object& value) {
    float& target = volume(state, kind, "set_volume");
    const bool valid = value.get_type() == sol::type::number && value.as<double>() >= 0.0 &&
                       value.as<double>() <= 1.0;
    if (!valid) {
        throw std::runtime_error(std::format(
            "{}audio.set_volume: volume must be a number from 0 to 1", caller_location(state)));
    }
    target = value.as<float>();
}

void ClientAudio::set_muted(sol::this_state state, const sol::object& muted) {
    if (muted.get_type() != sol::type::boolean) {
        throw std::runtime_error(
            std::format("{}audio.set_muted: argument must be a boolean", caller_location(state)));
    }
    _settings.muted = muted.as<bool>();
}

} // namespace engine::script
