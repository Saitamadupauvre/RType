#pragma once

#include "engine/core/InputState.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/IInput.hpp"

namespace engine::graphics {

/**
 * @brief Reads every key and mouse button held this frame, and the mouse position.
 *
 * @param input Keyboard and mouse of a graphics backend, updated by IRenderer::begin_frame().
 * @return The keys and mouse buttons down, and the mouse position in virtual units as the
 * pointer, outside [0, virtual_size()] when over the letterbox bars.
 * @note Main thread only, like @p input.
 */
[[nodiscard]] ENGINE_GRAPHICS_EXPORT core::InputState capture_input(const IInput& input);

} // namespace engine::graphics
