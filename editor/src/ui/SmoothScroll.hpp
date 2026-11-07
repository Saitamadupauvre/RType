#pragma once

#include <clay.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace editor {

/**
 * @brief Eases vertical scroll containers toward a wheel-driven target instead of Clay's
 * instant 10 px jump.
 *
 * Containers register while they are drawn; the next frame's update() moves the hovered one.
 * Clay must still run Clay_UpdateScrollContainers, with a zero delta, to track the containers.
 */
class SmoothScroll {
public:
    /** @brief Pixels scrolled per wheel notch, before the UI scale. */
    static constexpr float step = 40.F;

    /** @brief Records a scroll container drawn this frame. */
    void add(Clay_ElementId id);

    /**
     * @brief Moves the target of the hovered container by `wheel` notches of `notch` pixels and
     * eases every container drawn last frame toward its target, then forgets that frame.
     * @param delta_time Seconds since the last frame; zero leaves the positions where they are.
     */
    void update(float wheel, float notch, float delta_time);

private:
    std::vector<Clay_ElementId> _drawn;
    std::unordered_map<std::uint32_t, float> _targets;
};

/**
 * @brief The clip of a vertical scroll container, registered with the frame's SmoothScroll. It
 * clips both axes by default, so wide content never widens the panel.
 * @note Clay grows the children of a horizontal clip to their widest content: a panel whose
 * rows must keep the panel width passes `clip_horizontally = false` and keeps its texts wrapped.
 */
[[nodiscard]] Clay_ClipElementConfig scroll_clip(SmoothScroll& scroll, Clay_ElementId id,
                                                bool clip_horizontally = true);

} // namespace editor
