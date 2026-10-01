#pragma once

#include <chrono>
#include <cstddef>
#include <functional>

#include "engine/core/Export.hpp"
#include "engine/core/time/IClock.hpp"

namespace engine::core::time {

/**
 * @brief Settings of a FixedStepLoop.
 */
struct FixedStepConfig {
    Duration step = std::chrono::nanoseconds(1'000'000'000 / 60);
    std::size_t max_steps_per_frame = 5;
};

/**
 * @brief Runs game logic at a fixed rate whatever the frame rate.
 *
 * Each call to run_frame measures the time elapsed on the clock since the previous call and
 * calls the update function once per whole step contained in it. The remainder is kept for the
 * next frame. At most max_steps_per_frame steps run per frame: the time that could not be
 * simulated is dropped, so a long pause never causes a spiral of death.
 *
 * @note A clock that goes backwards is treated as no time elapsed. Not thread safe.
 */
class ENGINE_CORE_EXPORT FixedStepLoop {
public:
    using Update = std::function<void(Duration)>;

    /**
     * @brief Creates a loop reading time from clock.
     *
     * @param clock Must outlive the loop.
     * @param config Step and catch-up cap.
     * @throws std::invalid_argument If the step is not positive or the cap is zero.
     */
    explicit FixedStepLoop(const IClock& clock, FixedStepConfig config = {});

    /**
     * @brief Runs every step due since the previous frame.
     *
     * @param update Called with the fixed step for each simulated step. If it throws, the
     * exception propagates and the steps not yet run are dropped.
     * @return The number of steps run.
     */
    std::size_t run_frame(const Update& update);

    /**
     * @brief Returns how far the current frame is between two steps.
     *
     * @return A value in [0, 1) to interpolate rendering between the last two states.
     */
    [[nodiscard]] double alpha() const;

    [[nodiscard]] Duration step() const;

private:
    const IClock* _clock;
    FixedStepConfig _config;
    Duration _last;
    Duration _accumulator{};
};

} // namespace engine::core::time
