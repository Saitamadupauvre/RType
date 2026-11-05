#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

[[nodiscard]] bool toggle(Ui& ui, Clay_ElementId id, Clay_String label, bool on);

} // namespace editor
