#pragma once

#include "engine/core/Export.hpp"
#include "engine/core/time/IClock.hpp"

namespace engine::core::time {

/**
 * @brief Clock backed by the operating system monotonic clock.
 *
 * @note Thread safe.
 */
class ENGINE_CORE_EXPORT SteadyClock final : public IClock {
public:
    SteadyClock();
    ~SteadyClock() override;

    [[nodiscard]] Duration now() const override;

private:
    Duration _origin;
};

} // namespace engine::core::time
