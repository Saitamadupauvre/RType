#pragma once

#include <vector>

#include "engine/app/Export.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/network/Client.hpp"
#include "engine/script/ClientApi.hpp"

namespace engine::app {

[[nodiscard]] ENGINE_APP_EXPORT bool is_camera(const network::EntitySnapshot& snapshot);

[[nodiscard]] ENGINE_APP_EXPORT core::CameraView
resolve_camera(const std::vector<network::EntitySnapshot>& entities,
               const script::CameraOverride& local, core::Vec2 viewport);

} // namespace engine::app
