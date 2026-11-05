#pragma once

#include <clay.h>

#include <cstdint>
#include <vector>

namespace editor {

/** @brief Keys the editor reacts to; the platform layer maps its own key codes to these. */
enum class Key : std::uint8_t { s, y, z, v, g, h, del };

/** @brief Input of one frame, filled by the platform layer and read by the UI. */
struct UiInput {
    Clay_Vector2 pointer{};
    bool primary_down = false;
    /** True only on the frame the primary button went down. */
    bool primary_pressed = false;
    Clay_Vector2 wheel{};
    float delta_time = 0.F;
    /** Keys that went down this frame. */
    std::vector<Key> keys_pressed{};
    /** Ctrl, or Cmd on macOS. */
    bool primary_modifier = false;
    bool shift = false;
};

} // namespace editor
