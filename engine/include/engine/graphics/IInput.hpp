#pragma once

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::graphics {

/**
 * @brief Raw keyboard and mouse state provided by a graphics backend.
 *
 * States are those of the current frame, updated by IRenderer::begin_frame().
 *
 * @note Main thread only. Key::Unknown is never down.
 */
class ENGINE_GRAPHICS_EXPORT IInput : core::NonCopyableNonMovable {
public:
    virtual ~IInput();

    /** @return true while the key is held. */
    [[nodiscard]] virtual bool is_key_down(Key key) const = 0;

    /** @return true only on the frame the key went down. */
    [[nodiscard]] virtual bool is_key_pressed(Key key) const = 0;

    /** @return true only on the frame the key went up. */
    [[nodiscard]] virtual bool is_key_released(Key key) const = 0;

    /**
     * @brief Takes the next key pressed this frame from the backend's queue.
     *
     * @return Key::Unknown when no mapped key was pressed. Keys the engine does not know are
     * skipped.
     * @note Consumes the key: calling it again returns the next one.
     */
    [[nodiscard]] virtual Key last_key_pressed() = 0;

    /**
     * @return The mouse position in virtual units; outside [0, virtual_size()] when over the
     * letterbox bars.
     */
    [[nodiscard]] virtual core::Vec2 mouse_position() const = 0;

    /** @return true while the mouse button is held. */
    [[nodiscard]] virtual bool is_mouse_down(MouseButton button) const = 0;

    /** @return true only on the frame the mouse button went down. */
    [[nodiscard]] virtual bool is_mouse_pressed(MouseButton button) const = 0;

    /** @return true only on the frame the mouse button went up. */
    [[nodiscard]] virtual bool is_mouse_released(MouseButton button) const = 0;
};

} // namespace engine::graphics
