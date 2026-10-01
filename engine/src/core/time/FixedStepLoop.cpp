#include "engine/core/time/FixedStepLoop.hpp"

#include <algorithm>
#include <stdexcept>

namespace engine::core::time {

FixedStepLoop::FixedStepLoop(const IClock& clock, FixedStepConfig config)
    : _clock(&clock), _config(config), _last(clock.now()) {
    if (_config.step <= Duration::zero()) {
        throw std::invalid_argument("FixedStepLoop step must be positive");
    }
    if (_config.max_steps_per_frame == 0) {
        throw std::invalid_argument("FixedStepLoop max_steps_per_frame must be positive");
    }
}

std::size_t FixedStepLoop::run_frame(const Update& update) {
    const Duration now = _clock->now();
    _accumulator += std::max(now - _last, Duration::zero());
    _last = now;

    std::size_t steps = 0;
    while (_accumulator >= _config.step && steps < _config.max_steps_per_frame) {
        _accumulator -= _config.step;
        ++steps;
        try {
            update(_config.step);
        } catch (...) {
            _accumulator = Duration::zero();
            throw;
        }
    }
    if (_accumulator >= _config.step) {
        _accumulator = Duration::zero();
    }
    return steps;
}

double FixedStepLoop::alpha() const {
    return static_cast<double>(_accumulator.count()) / static_cast<double>(_config.step.count());
}

Duration FixedStepLoop::step() const { return _config.step; }

} // namespace engine::core::time
