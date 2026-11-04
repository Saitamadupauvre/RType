#pragma once

#include <clay.h>

#include "ui/Theme.hpp"

namespace editor {

void text(const Theme& theme, Clay_String content, float size, Clay_Color color,
          Face face = Face::regular);

void spacer();

} // namespace editor
