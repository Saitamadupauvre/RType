#include "ui/ClickCounter.hpp"

#include <cmath>
#include <limits>

namespace editor {

std::uint8_t ClickCounter::press(Clay_Vector2 position, double time) {
    const bool series =
        continues(position, time) && _count < std::numeric_limits<std::uint8_t>::max();
    _count = series ? static_cast<std::uint8_t>(_count + 1) : std::uint8_t{1};
    _last = Press{.position = position, .time = time};
    return _count;
}

bool ClickCounter::continues(Clay_Vector2 position, double time) const {
    if (!_last) {
        return false;
    }
    const float distance =
        std::hypot(position.x - _last->position.x, position.y - _last->position.y);
    const double elapsed = time - _last->time;
    return elapsed >= 0.0 && elapsed <= max_interval && distance <= max_distance;
}

} // namespace editor
