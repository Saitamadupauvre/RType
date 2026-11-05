#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

struct RowContent {
    Clay_String label;
    Clay_String detail{};
    bool selected = false;
};

void list_item(const Theme& theme, const RowContent& content);

[[nodiscard]] bool list_row(Ui& ui, Clay_ElementId id, const RowContent& content);

void empty_hint(const Theme& theme, Clay_String hint);

} // namespace editor
