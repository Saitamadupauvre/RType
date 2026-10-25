#pragma once

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"

namespace engine::graphics {

/**
 * @brief Everything a graphics plugin provides, created by its engine_graphics_create().
 *
 * @note The references returned stay valid for the lifetime of the backend. Main thread only.
 */
class ENGINE_GRAPHICS_EXPORT IGraphicsBackend : core::NonCopyableNonMovable {
public:
    virtual ~IGraphicsBackend();

    /** @return The window and frame interface. */
    virtual IRenderer& renderer() = 0;

    /** @return The keyboard interface. */
    virtual IInput& input() = 0;
};

} // namespace engine::graphics
