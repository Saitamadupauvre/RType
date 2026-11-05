#pragma once

#include "engine/app/Export.hpp"
#include "engine/core/console/Console.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::app {

ENGINE_APP_EXPORT void draw_console(graphics::IRenderer& renderer,
                                    const core::console::Console& console);

} // namespace engine::app
