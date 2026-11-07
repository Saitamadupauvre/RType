#pragma once

#include "text/TextEdit.hpp"
#include "ui/Platform.hpp"
#include "ui/UiInput.hpp"

namespace editor {

void apply_editing_keys(const UiInput& input, const Platform& platform, TextEdit& draft);

} // namespace editor
