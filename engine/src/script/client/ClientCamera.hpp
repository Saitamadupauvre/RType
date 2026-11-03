#pragma once

#include <string_view>
#include <tuple>

#include "engine/core/Camera.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/ClientApi.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class ClientCamera {
public:
    ClientCamera();

    void bind(sol::state& lua);

    [[nodiscard]] CameraOverride override_view() const noexcept { return _override; }
    void set_view(core::CameraView view, core::Vec2 viewport) noexcept;

private:
    void set(sol::this_state state, const sol::object& x, const sol::object& y,
             const sol::object& zoom);
    void follow(sol::this_state state, const sol::object& id, const sol::object& zoom);
    [[nodiscard]] std::tuple<float, float>
    screen_to_world(sol::this_state state, const sol::object& x, const sol::object& y) const;
    [[nodiscard]] float zoom_or_current(sol::this_state state, const sol::object& zoom,
                                        std::string_view function) const;

    CameraOverride _override;
    core::CameraView _view;
    core::Vec2 _viewport;
};

} // namespace engine::script
