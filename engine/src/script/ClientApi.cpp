#include "engine/script/ClientApi.hpp"

#include <exception>
#include <optional>
#include <utility>

#include "RuntimeContext.hpp"
#include "client/ClientAudio.hpp"
#include "client/ClientCamera.hpp"
#include "client/ClientEvents.hpp"
#include "client/LocalSpawn.hpp"
#include "engine/core/log/Log.hpp"
#include "input/LuaInput.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

} // namespace

struct ClientApi::State {
    std::optional<ClientEvents> events;
    ClientInput input;
    ClientAudio audio;
    ClientCamera camera;
};

ClientApi::ClientApi() : _state(std::make_unique<State>()) {}

ClientApi::~ClientApi() = default;

std::string_view ClientApi::section() const noexcept { return "client"; }

void ClientApi::install(RuntimeContext& context) {
    bind_client_input(context.types);
    context.lua["input"] = &_state->input;
    _state->audio.bind(context.lua);
    _state->camera.bind(context.lua);
    _state->events.emplace(context).bind();
    bind_spawn_local(context);
}

bool ClientApi::dispatch(const EventCommand& event) noexcept {
    if (!_state->events) {
        return true;
    }
    try {
        return _state->events->dispatch(event);
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

AudioSettings ClientApi::audio_settings() const noexcept { return _state->audio.settings(); }

void ClientApi::set_audio_settings(AudioSettings settings) noexcept {
    _state->audio.set_settings(settings);
}

CameraOverride ClientApi::camera_override() const noexcept {
    return _state->camera.override_view();
}

void ClientApi::set_camera_view(core::CameraView view, core::Vec2 viewport) noexcept {
    _state->camera.set_view(view, viewport);
}

} // namespace engine::script
