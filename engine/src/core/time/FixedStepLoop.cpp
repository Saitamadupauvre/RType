#include "engine/core/time/FixedStepLoop.hpp"

#include <algorithm>
#include <stdexcept>

namespace engine::core::time {

FixedStepLoop::FixedStepLoop(const IClock& clock, FixedStepConfig config)
    : clock_(&clock), config_(config), last_(clock.now()) {
    if (config_.step <= Duration::zero()) {
        throw std::invalid_argument("FixedStepLoop step must be positive");
    }
    if (config_.max_steps_per_frame == 0) {
        throw std::invalid_argument("FixedStepLoop max_steps_per_frame must be positive");
    }
}

std::size_t FixedStepLoop::run_frame(const Update& update) {
    const Duration now = clock_->now();
    accumulator_ += std::max(now - last_, Duration::zero());
    last_ = now;

    std::size_t steps = 0;
    while (accumulator_ >= config_.step && steps < config_.max_steps_per_frame) {
        accumulator_ -= config_.step;
        ++steps;
        try {
            update(config_.step);
        } catch (...) {
            accumulator_ = Duration::zero();
            throw;
        }
    }
    if (accumulator_ >= config_.step) {
        accumulator_ = Duration::zero();
    }
    return steps;
}

double FixedStepLoop::alpha() const {
    return static_cast<double>(accumulator_.count()) / static_cast<double>(config_.step.count());
}

Duration FixedStepLoop::step() const { return config_.step; }

} // namespace engine::core::time
