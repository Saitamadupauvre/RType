#pragma once

#include <chrono>

#include "engine/core/Export.hpp"

namespace engine::core::time {

using Duration = std::chrono::nanoseconds;

/**
 * @brief Source of monotonic time, injected wherever the engine needs to measure elapsed time.
 */
class ENGINE_CORE_EXPORT IClock {
public:
    IClock() = default;
    virtual ~IClock();

    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    IClock(IClock&&) = delete;
    IClock& operator=(IClock&&) = delete;

    /**
     * @brief Returns the time elapsed since an arbitrary, fixed origin.
     *
     * @return A value that never decreases between two calls on the same clock.
     */
    [[nodiscard]] virtual Duration now() const = 0;
};

} // namespace engine::core::time
