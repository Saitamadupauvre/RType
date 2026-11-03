#include "engine/script/ClientApi.hpp"

#include <exception>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "RuntimeContext.hpp"
#include "client/ClientAudio.hpp"
#include "client/ClientCamera.hpp"
#include "client/ClientEvents.hpp"
#include "client/HudCanvas.hpp"
#include "client/LocalSpawn.hpp"
#include "engine/core/log/Log.hpp"
#include "input/LuaInput.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

constexpr std::string_view draw_hud_callback = "draw_hud";

} // namespace

struct ClientApi::State {
    RuntimeContext* context = nullptr;
    std::optional<ClientEvents> events;
    std::optional<HudCanvas> hud;
    ClientInput input;
    ClientAudio audio;
    ClientCamera camera;
};

ClientApi::ClientApi() : _state(std::make_unique<State>()) {}

ClientApi::~ClientApi() = default;

std::string_view ClientApi::section() const noexcept { return "client"; }

void ClientApi::install(RuntimeContext& context) {
    _state->context = &context;
    bind_client_input(context.types);
    context.lua["input"] = &_state->input;
    _state->audio.bind(context.lua);
    _state->camera.bind(context.lua);
    _state->events.emplace(context).bind();
    bind_spawn_local(context);
    _state->hud.emplace(context.textures).bind(context.lua);
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

std::vector<HudCommand> ClientApi::draw_hud() noexcept {
    if (_state->context == nullptr || !_state->hud) {
        return {};
    }
    _state->hud->open();
    try {
        const auto callback = _state->context->main_callback(draw_hud_callback);
        if (callback.valid()) {
            report(callback());
        }
    } catch (const std::exception& exception) {
        log::error("lua: draw_hud: {}", exception.what());
    } catch (...) {
        log::error("lua: draw_hud: unknown error");
    }
    return _state->hud->close();
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
