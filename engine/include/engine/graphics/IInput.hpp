#pragma once

#include "engine/core/NonCopyableNonMovable.hpp"
#include "engine/graphics/Export.hpp"
#include "engine/graphics/Types.hpp"

namespace engine::graphics {

/**
 * @brief Raw keyboard state provided by a graphics backend.
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
};

} // namespace engine::graphics
