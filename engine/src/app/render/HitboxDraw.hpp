#pragma once

#include "engine/app/Export.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

ENGINE_APP_EXPORT void draw_hitboxes(graphics::IRenderer& renderer, core::EntityManager& world);

} // namespace engine::app
