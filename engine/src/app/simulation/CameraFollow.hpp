#pragma once

#include "engine/app/Export.hpp"
#include "engine/core/ecs/EntityManager.hpp"

namespace engine::app {

ENGINE_APP_EXPORT void follow_cameras(core::EntityManager& world);

} // namespace engine::app
