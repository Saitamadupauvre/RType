#include "engine/script/ClientApi.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/log/Log.hpp"
#include "input/LuaInput.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

AudioSettings clamped(AudioSettings settings) {
    settings.music_volume = std::clamp(settings.music_volume, 0.F, 1.F);
    settings.effects_volume = std::clamp(settings.effects_volume, 0.F, 1.F);
    return settings;
}

} // namespace

struct ClientApi::State {
    RuntimeContext* context = nullptr;
    std::map<std::string, std::vector<sol::protected_function>, std::less<>> handlers;
    ClientInput input;
    AudioSettings audio;

    void on(sol::this_state state, const sol::object& name, const sol::object& handler);
    bool dispatch(const EventCommand& event);
    void bind_audio();
    float& volume(sol::this_state state, const sol::object& kind, std::string_view function);
    void set_volume(sol::this_state state, const sol::object& kind, const sol::object& volume);
    void set_muted(sol::this_state state, const sol::object& muted);
};

void ClientApi::State::on(sol::this_state state, const sol::object& name,
                          const sol::object& handler) {
    if (name.get_type() != sol::type::string) {
        throw std::runtime_error(
            std::format("{}on: event name must be a string", caller_location(state)));
    }
    auto event = name.as<std::string>();
    if (!context->events.contains(event)) {
        throw std::runtime_error(std::format("{}on: unknown event '{}', declare it with event",
                                             caller_location(state), event));
    }
    if (handler.get_type() != sol::type::function) {
        throw std::runtime_error(std::format("{}on: handler of event '{}' must be a function",
                                             caller_location(state), event));
    }
    handlers[std::move(event)].push_back(handler.as<sol::protected_function>());
}

bool ClientApi::State::dispatch(const EventCommand& event) {
    const auto found = handlers.find(event.name);
    if (found == handlers.end()) {
        return true;
    }
    const sol::this_state state{context->lua.lua_state()};
    bool success = true;
    for (const sol::protected_function& handler : found->second) {
        sol::table arguments = context->lua.create_table();
        for (const EventArgument& argument : event.arguments) {
            arguments[argument.key] = to_lua(state, argument.value);
        }
        success = report(handler(arguments)) && success;
    }
    return success;
}

void ClientApi::State::bind_audio() {
    sol::table table = context->lua.create_named_table("audio");
    table.set_function("muted", [this] { return audio.muted; });
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

float& ClientApi::State::volume(sol::this_state state, const sol::object& kind,
                                std::string_view function) {
    if (kind.get_type() == sol::type::string) {
        const auto name = kind.as<std::string_view>();
        if (name == "music") {
            return audio.music_volume;
        }
        if (name == "effects") {
            return audio.effects_volume;
        }
    }
    throw std::runtime_error(std::format(R"({}audio.{}: kind must be "music" or "effects")",
                                         caller_location(state), function));
}

void ClientApi::State::set_volume(sol::this_state state, const sol::object& kind,
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

void ClientApi::State::set_muted(sol::this_state state, const sol::object& muted) {
    if (muted.get_type() != sol::type::boolean) {
        throw std::runtime_error(
            std::format("{}audio.set_muted: argument must be a boolean", caller_location(state)));
    }
    audio.muted = muted.as<bool>();
}

ClientApi::ClientApi() : _state(std::make_unique<State>()) {}

ClientApi::~ClientApi() = default;

std::string_view ClientApi::section() const noexcept { return "client"; }

void ClientApi::install(RuntimeContext& context) {
    _state->context = &context;
    bind_client_input(context.types);
    context.lua["input"] = &_state->input;
    _state->bind_audio();
    context.lua.set_function(
        "on", [this](sol::this_state state, const sol::object& name, const sol::object& handler) {
            _state->on(state, name, handler);
        });
}

bool ClientApi::dispatch(const EventCommand& event) noexcept {
    try {
        return _state->dispatch(event);
    } catch (const std::exception& exception) {
        log::error("lua: event '{}': {}", event.name, exception.what());
    } catch (...) {
        log::error("lua: event '{}': unknown error", event.name);
    }
    return false;
}

void ClientApi::set_input(core::InputState input) noexcept {
    _state->input.previous = std::exchange(_state->input.current, std::move(input));
}

AudioSettings ClientApi::audio_settings() const noexcept { return _state->audio; }

void ClientApi::set_audio_settings(AudioSettings settings) noexcept {
    _state->audio = clamped(settings);
}

} // namespace engine::script
