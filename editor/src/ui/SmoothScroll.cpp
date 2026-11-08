#include "ui/SmoothScroll.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace editor {

namespace {

constexpr float speed = 15.F;
constexpr float snap_distance = 0.5F;

float ease(float position, float target, float delta_time) {
    const float next = position + ((target - position) * (1.F - std::exp(-speed * delta_time)));
    return std::abs(target - next) < snap_distance ? target : next;
}

} // namespace

void SmoothScroll::add(Clay_ElementId id) { _drawn.push_back(id); }

void SmoothScroll::update(float wheel, float notch, float delta_time) {
    std::unordered_map<std::uint32_t, float> targets;
    for (const Clay_ElementId id : _drawn) {
        const Clay_ScrollContainerData data = Clay_GetScrollContainerData(id);
        if (!data.found) {
            continue;
        }
        float& position = data.scrollPosition->y;
        const auto known = _targets.find(id.id);
        float target = known == _targets.end() ? position : known->second;
        if (wheel != 0.F && Clay_PointerOver(id)) {
            target += wheel * notch;
        }
        const float end =
            std::max(data.contentDimensions.height - data.scrollContainerDimensions.height, 0.F);
        target = std::clamp(target, -end, 0.F);
        position = ease(position, target, delta_time);
        targets[id.id] = target;
    }
    _targets = std::move(targets);
    _drawn.clear();
}

Clay_ClipElementConfig scroll_clip(SmoothScroll& scroll, Clay_ElementId id) {
    scroll.add(id);
    return {.horizontal = false, .vertical = true, .childOffset = Clay_GetScrollOffset()};
}

} // namespace editor
