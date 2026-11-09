#pragma once

#include <clay.h>

#include "ui/Ui.hpp"

namespace editor {

struct RowContent {
    Clay_String label;
    Clay_String detail{};
    bool selected = false;
};

/** @brief Draws a row that cannot be clicked. */
void list_item(const Theme& theme, const RowContent& content);

/** @brief Draws a row that can be clicked. @return Whether it was clicked this frame. */
[[nodiscard]] bool list_row(Ui& ui, Clay_ElementId id, const RowContent& content);

/** @brief Draws the dim line shown in place of an empty list. */
void empty_hint(const Theme& theme, Clay_String hint);

} // namespace editor
