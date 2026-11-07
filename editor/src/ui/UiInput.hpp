#pragma once

#include <clay.h>

#include <cstdint>
#include <string>
#include <vector>

namespace editor {

/** @brief Keys the editor reacts to; the platform layer maps its own key codes to these. */
enum class Key : std::uint8_t {
    a,
    c,
    s,
    v,
    x,
    y,
    z,
    g,
    h,
    del,
    backspace,
    enter,
    escape,
    tab,
    left,
    right,
    up,
    down,
    home,
    end,
};

/** @brief Input of one frame, filled by the platform layer and read by the UI. */
struct UiInput {
    Clay_Vector2 pointer{};
    bool primary_down = false;
    bool middle_down = false;
    /** True only on the frame the primary button went down. */
    bool primary_pressed = false;
    /** Rank of this frame's press in a series of quick clicks: 2 for a double click. */
    std::uint8_t click_count = 1;
    Clay_Vector2 wheel{};
    float delta_time = 0.F;
    /** Keys that went down this frame. */
    std::vector<Key> keys_pressed{};
    /** Keys held long enough to repeat this frame. */
    std::vector<Key> keys_repeated{};
    /** Text typed this frame, in UTF-8. */
    std::string typed{};
    /** Ctrl, or Cmd on macOS. */
    bool primary_modifier = false;
    bool shift = false;
    bool alt = false;
};

/** @brief Whether the key went down or repeated this frame. */
[[nodiscard]] bool pressed_or_repeated(const UiInput& input, Key key);

} // namespace editor
