#pragma once

#include <clay.h>

#include "ui/Theme.hpp"

namespace editor {

Clay_RenderCommandArray build_layout(const Theme& theme, Clay_Dimensions window);

} // namespace editor
