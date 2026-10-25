#include "engine/core/time/ManualClock.hpp"

#include <stdexcept>

namespace engine::core::time {

ManualClock::ManualClock() = default;

ManualClock::~ManualClock() = default;

Duration ManualClock::now() const { return _current; }

void ManualClock::advance(Duration delta) {
    if (delta < Duration::zero()) {
        throw std::invalid_argument("ManualClock cannot go backwards");
    }
    _current += delta;
}

} // namespace engine::core::time
