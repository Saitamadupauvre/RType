#pragma once

#include <cstddef>

#include "engine/app/Export.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

ENGINE_APP_EXPORT void draw_loading_screen(graphics::IRenderer& renderer, std::size_t loaded,
                                           std::size_t total);

} // namespace engine::app
