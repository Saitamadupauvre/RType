#include "engine/core/time/ManualClock.hpp"

#include <stdexcept>

namespace engine::core::time {

ManualClock::ManualClock() = default;

ManualClock::~ManualClock() = default;

Duration ManualClock::now() const { return current_; }

void ManualClock::advance(Duration delta) {
    if (delta < Duration::zero()) {
        throw std::invalid_argument("ManualClock cannot go backwards");
    }
    current_ += delta;
}

} // namespace engine::core::time
