#include "text/FitText.hpp"

#include <cstddef>

#include "text/Utf8.hpp"

namespace editor {

std::string fit_text(std::string_view text, float width, const MeasureWidth& measure) {
    if (measure(text) <= width) {
        return std::string(text);
    }
    std::size_t end = text.size();
    while (end > 0) {
        end = previous_boundary(text, end);
        std::string shortened(text.substr(0, end));
        shortened += ellipsis;
        if (measure(shortened) <= width) {
            return shortened;
        }
    }
    return std::string(ellipsis);
}

} // namespace editor
