#pragma once

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::graphics {

/**
 * @brief Window and frame management provided by a graphics backend.
 *
 * Coordinates are virtual units (see virtual_size()); the backend scales them to the window.
 *
 * @note Every member function must be called from the main thread, the one that called open().
 */
class ENGINE_GRAPHICS_EXPORT IRenderer : core::NonCopyableNonMovable {
public:
    virtual ~IRenderer();

    /**
     * @brief Opens the window.
     *
     * @param config Size, title and fullscreen mode.
     * @return false if the window could not be created; the renderer stays closed.
     */
    virtual bool open(const WindowConfig& config) = 0;

    /** @brief Closes the window. Harmless if it is not open. */
    virtual void close() = 0;

    /** @return true once the user asked to close the window, or if it is not open. */
    [[nodiscard]] virtual bool should_close() const = 0;

    /** @param enabled true for fullscreen, false for windowed. */
    virtual void set_fullscreen(bool enabled) = 0;

    /** @return The size of the drawing space in virtual units, whatever the window size. */
    [[nodiscard]] virtual Vec2 virtual_size() const = 0;

    /**
     * @brief Starts a frame and clears it.
     *
     * @param clear Color of the whole window, letterbox bars included.
     */
    virtual void begin_frame(Color clear) = 0;

    /** @brief Presents the frame started by begin_frame(). */
    virtual void end_frame() = 0;
};

} // namespace engine::graphics
