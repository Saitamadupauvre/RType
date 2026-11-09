#pragma once

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/** @brief How long a refusal stays over the viewport, in seconds. */
inline constexpr float toast_seconds = 3.F;

/**
 * @brief Shows the last refused action in red at the bottom of the viewport for toast_seconds,
 * so a refusal is seen even when the console is hidden.
 */
void refusal_toast(Ui& ui, const Editor& editor);

/** @brief Shows what is being dragged (an asset or a prefab) next to the pointer. */
void drag_chip(Ui& ui);

} // namespace editor
