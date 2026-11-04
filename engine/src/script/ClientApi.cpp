#include "engine/script/ClientApi.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/log/Log.hpp"
#include "input/LuaInput.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr core::Vec2 default_viewport{1920.F, 1080.F};

[[noreturn]] void camera_error(sol::this_state state, std::string_view function,
                               std::string_view message) {
    throw std::runtime_error(
        std::format("{}camera.{}: {}", caller_location(state), function, message));
}

float camera_coordinate(sol::this_state state, const sol::object& value, std::string_view function,
                        std::string_view what) {
    if (value.get_type() != sol::type::number || !std::isfinite(value.as<double>())) {
        camera_error(state, function, std::format("{} must be a finite number", what));
    }
    return value.as<float>();
}

float camera_zoom(sol::this_state state, const sol::object& value, std::string_view function,
                  float current) {
    if (value.get_type() == sol::type::lua_nil || value.get_type() == sol::type::none) {
        return current;
    }
    const bool valid = value.get_type() == sol::type::number && std::isfinite(value.as<double>()) &&
                       value.as<double>() > 0.0;
    if (!valid) {
        camera_error(state, function, "zoom must be a positive number");
    }
    return value.as<float>();
}

core::Entity::Id camera_entity(sol::this_state state, const sol::object& value) {
    if (value.get_type() == sol::type::number) {
        lua_State* lua = state;
        value.push(lua);
        const bool integer = lua_isinteger(lua, -1) != 0;
        lua_pop(lua, 1);
        if (integer && value.as<lua_Integer>() >= 0) {
            return static_cast<core::Entity::Id>(value.as<lua_Integer>());
        }
    }
    camera_error(state, "follow", "id must be a non-negative integer");
}

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
    CameraOverride camera;
    core::CameraView view = core::default_view(default_viewport);
    core::Vec2 viewport = default_viewport;

    void on(sol::this_state state, const sol::object& name, const sol::object& handler);
    [[nodiscard]] bool notify_scene_loaded(const EventCommand& event) const;
    bool dispatch(const EventCommand& event);
    void bind_audio();
    void bind_camera();
    void set_camera(sol::this_state state, const sol::object& x, const sol::object& y,
                    const sol::object& zoom);
    void follow(sol::this_state state, const sol::object& id, const sol::object& zoom);
    [[nodiscard]] std::tuple<float, float>
    screen_to_world(sol::this_state state, const sol::object& x, const sol::object& y) const;
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

bool ClientApi::State::notify_scene_loaded(const EventCommand& event) const {
    if (event.name != scene_loaded_event) {
        return true;
    }
    const sol::protected_function callback = context->main_callback("on_scene_loaded");
    if (!callback.valid()) {
        return true;
    }
    for (const EventArgument& argument : event.arguments) {
        const auto* scene = std::get_if<std::string>(&argument.value);
        if (argument.key == scene_loaded_key && scene != nullptr) {
            return report(callback(*scene));
        }
    }
    return true;
}

bool ClientApi::State::dispatch(const EventCommand& event) {
    bool success = notify_scene_loaded(event);
    const auto found = handlers.find(event.name);
    if (found == handlers.end()) {
        return success;
    }
    const sol::this_state state{context->lua.lua_state()};
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

void ClientApi::State::bind_camera() {
    sol::table table = context->lua.create_named_table("camera");
    table.set_function("set",
                       [this](sol::this_state state, const sol::object& x, const sol::object& y,
                              const sol::object& zoom) { set_camera(state, x, y, zoom); });
    table.set_function("follow", [this](sol::this_state state, const sol::object& id,
                                        const sol::object& zoom) { follow(state, id, zoom); });
    table.set_function("reset", [this] { camera = CameraOverride{}; });
    table.set_function("screen_to_world",
                       [this](sol::this_state state, const sol::object& x, const sol::object& y) {
                           return screen_to_world(state, x, y);
                       });
}

void ClientApi::State::set_camera(sol::this_state state, const sol::object& x, const sol::object& y,
                                  const sol::object& zoom) {
    const core::Vec2 center{camera_coordinate(state, x, "set", "x"),
                            camera_coordinate(state, y, "set", "y")};
    const float scale = camera_zoom(state, zoom, "set", view.zoom);
    camera = CameraOverride{.kind = CameraOverride::Kind::Fixed,
                            .view = {.center = center, .zoom = scale},
                            .entity = 0};
}

void ClientApi::State::follow(sol::this_state state, const sol::object& id,
                              const sol::object& zoom) {
    const core::Entity::Id entity = camera_entity(state, id);
    const float scale = camera_zoom(state, zoom, "follow", view.zoom);
    camera = CameraOverride{.kind = CameraOverride::Kind::Follow,
                            .view = {.center = view.center, .zoom = scale},
                            .entity = entity};
}

std::tuple<float, float> ClientApi::State::screen_to_world(sol::this_state state,
                                                           const sol::object& x,
                                                           const sol::object& y) const {
    const core::Vec2 point{camera_coordinate(state, x, "screen_to_world", "x"),
                           camera_coordinate(state, y, "screen_to_world", "y")};
    const core::Vec2 world = core::screen_to_world(view, viewport, point);
    return {world.x, world.y};
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
    _state->bind_camera();
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

CameraOverride ClientApi::camera_override() const noexcept { return _state->camera; }

void ClientApi::set_camera_view(core::CameraView view, core::Vec2 viewport) noexcept {
    _state->view = view;
    _state->viewport = viewport;
}

} // namespace engine::script
