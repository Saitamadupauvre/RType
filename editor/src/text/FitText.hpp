#pragma once

#include <string>
#include <string_view>

#include "text/TextEdit.hpp"

namespace editor {

inline constexpr std::string_view ellipsis = "...";

/**
 * @brief The text when it fits in `width`, or its longest start that fits followed by "...".
 * Cuts only between UTF-8 code points.
 */
[[nodiscard]] std::string fit_text(std::string_view text, float width, const MeasureWidth& measure);

} // namespace editor
