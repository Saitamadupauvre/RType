#pragma once

#include <clay.h>

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

Clay_RenderCommandArray build_frame(Ui& ui, const Editor& editor, Clay_Dimensions window);

} // namespace editor
