#include "text/CaretScroll.hpp"

#include <algorithm>

namespace editor {

float scroll_to_caret(float offset, float caret_x, float text_width, float visible) noexcept {
    const float room = std::max(visible, 0.F);
    if (caret_x < offset) {
        offset = caret_x;
    } else if (caret_x > offset + room) {
        offset = caret_x - room;
    }
    return std::clamp(offset, 0.F, std::max(text_width - room, 0.F));
}

} // namespace editor
