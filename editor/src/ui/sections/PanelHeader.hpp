#pragma once

#include <clay.h>

#include "ui/Theme.hpp"

namespace editor {

/** @brief A panel title in capitals, with an optional count or note on the right. */
void panel_header(const Theme& theme, Clay_String title, Clay_String detail = {});

} // namespace editor
