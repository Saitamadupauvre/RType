#include "engine/core/time/SteadyClock.hpp"

#include <chrono>

namespace engine::core::time {

namespace {

Duration steady_now() {
    return std::chrono::duration_cast<Duration>(
        std::chrono::steady_clock::now().time_since_epoch());
}

} // namespace

SteadyClock::SteadyClock() : origin_(steady_now()) {}

SteadyClock::~SteadyClock() = default;

Duration SteadyClock::now() const { return steady_now() - origin_; }

} // namespace engine::core::time
