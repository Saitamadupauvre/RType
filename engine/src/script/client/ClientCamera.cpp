#include "client/ClientCamera.hpp"

#include <format>
#include <stdexcept>
#include <string>

#include "lua/LuaArgs.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

constexpr core::Vec2 default_viewport{1920.F, 1080.F};

[[noreturn]] void camera_error(sol::this_state state, std::string_view function,
                               std::string_view message) {
    throw std::runtime_error(
        std::format("{}camera.{}: {}", caller_location(state), function, message));
}

float coordinate(sol::this_state state, const sol::object& value, std::string_view function,
                 std::string_view what) {
    const auto number = finite_number(value);
    if (!number) {
        camera_error(state, function, std::format("{} must be a finite number", what));
    }
    return *number;
}

} // namespace

ClientCamera::ClientCamera()
    : _view(core::default_view(default_viewport)), _viewport(default_viewport) {}

void ClientCamera::bind(sol::state& lua) {
    sol::table table = lua.create_named_table("camera");
    table.set_function("set",
                       [this](sol::this_state state, const sol::object& x, const sol::object& y,
                              const sol::object& zoom) { set(state, x, y, zoom); });
    table.set_function("follow", [this](sol::this_state state, const sol::object& id,
                                        const sol::object& zoom) { follow(state, id, zoom); });
    table.set_function("reset", [this] { _override = CameraOverride{}; });
    table.set_function("screen_to_world",
                       [this](sol::this_state state, const sol::object& x, const sol::object& y) {
                           return screen_to_world(state, x, y);
                       });
}

void ClientCamera::set_view(core::CameraView view, core::Vec2 viewport) noexcept {
    _view = view;
    _viewport = viewport;
}

float ClientCamera::zoom_or_current(sol::this_state state, const sol::object& zoom,
                                    std::string_view function) const {
    if (is_nil(zoom)) {
        return _view.zoom;
    }
    const auto scale = positive_number(zoom);
    if (!scale) {
        camera_error(state, function, "zoom must be a positive number");
    }
    return *scale;
}

void ClientCamera::set(sol::this_state state, const sol::object& x, const sol::object& y,
                       const sol::object& zoom) {
    const core::Vec2 center{coordinate(state, x, "set", "x"), coordinate(state, y, "set", "y")};
    const float scale = zoom_or_current(state, zoom, "set");
    _override = CameraOverride{.kind = CameraOverride::Kind::Fixed,
                               .view = {.center = center, .zoom = scale},
                               .entity = 0};
}

void ClientCamera::follow(sol::this_state state, const sol::object& id, const sol::object& zoom) {
    const auto entity = non_negative_integer(state, id);
    if (!entity) {
        camera_error(state, "follow", "id must be a non-negative integer");
    }
    const float scale = zoom_or_current(state, zoom, "follow");
    _override = CameraOverride{.kind = CameraOverride::Kind::Follow,
                               .view = {.center = _view.center, .zoom = scale},
                               .entity = static_cast<core::Entity::Id>(*entity)};
}

std::tuple<float, float> ClientCamera::screen_to_world(sol::this_state state, const sol::object& x,
                                                       const sol::object& y) const {
    const core::Vec2 point{coordinate(state, x, "screen_to_world", "x"),
                           coordinate(state, y, "screen_to_world", "y")};
    const core::Vec2 world = core::screen_to_world(_view, _viewport, point);
    return {world.x, world.y};
}

} // namespace engine::script
