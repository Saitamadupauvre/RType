#pragma once

#include "engine/core/Export.hpp"
#include "engine/core/time/IClock.hpp"

namespace engine::core::time {

/**
 * @brief Clock that only moves when told to, so tests are deterministic.
 */
class ENGINE_CORE_EXPORT ManualClock final : public IClock {
public:
    ManualClock();
    ~ManualClock() override;

    [[nodiscard]] Duration now() const override;

    /**
     * @brief Moves the clock forward.
     *
     * @param delta The time to add.
     * @throws std::invalid_argument If delta is negative.
     */
    void advance(Duration delta);

private:
    Duration _current{};
};

} // namespace engine::core::time
