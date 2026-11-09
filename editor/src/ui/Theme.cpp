#include "ui/Theme.hpp"

#include <cmath>

namespace editor {

float Theme::px(float design) const { return design * scale; }

std::uint16_t Theme::px16(float design) const {
    return static_cast<std::uint16_t>(std::lround(px(design)));
}

} // namespace editor
