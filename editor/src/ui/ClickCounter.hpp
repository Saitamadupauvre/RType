#pragma once

#include <clay.h>

#include <cstdint>
#include <optional>

namespace editor {

/** @brief Counts consecutive presses close in time and space: 1, 2 for a double click... */
class ClickCounter {
public:
    static constexpr double max_interval = 0.4;
    static constexpr float max_distance = 4.F;

    /** @brief Records a press at `time` seconds and returns its rank in the series. */
    std::uint8_t press(Clay_Vector2 position, double time);

private:
    struct Press {
        Clay_Vector2 position;
        double time;
    };

    [[nodiscard]] bool continues(Clay_Vector2 position, double time) const;

    std::optional<Press> _last;
    std::uint8_t _count = 0;
};

} // namespace editor
