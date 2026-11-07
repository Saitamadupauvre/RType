#pragma once

#include <clay.h>

#include <string_view>

#include "ui/Theme.hpp"

namespace editor {

void text(const Theme& theme, Clay_String content, float size, Clay_Color color,
          Face face = Face::regular);

void spacer();

[[nodiscard]] Clay_String view_of(std::string_view stable);

} // namespace editor
