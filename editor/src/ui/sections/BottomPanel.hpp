#pragma once

#include "action/Editor.hpp"
#include "ui/Ui.hpp"

namespace editor {

/** @brief The bottom panel's unscaled height bounds: half and twice its design height. */
inline constexpr float bottom_panel_least = size::bottom_panel / 2.F;
inline constexpr float bottom_panel_most = size::bottom_panel * 2.F;

/**
 * @brief The bottom panel height for a window `window_height` pixels tall: @p chosen (unscaled,
 * kept between bottom_panel_least and bottom_panel_most), shrunk down to size::bottom_panel_min
 * when the workspace would get less than its own minimum. It depends on the window and the
 * user's choice only, never on what the panels show.
 */
[[nodiscard]] float bottom_panel_height(const Theme& theme, float window_height,
                                        float chosen = size::bottom_panel);

/**
 * @brief Follows a drag of the grip on the panel's top edge: dragging up makes the panel taller.
 * A double-click on the grip goes back to the design height. Asks for the vertical resize cursor
 * while the grip is hovered or dragged.
 */
void resize_bottom_panel(Ui& ui);

void bottom_panel(Ui& ui, const Editor& editor, float height);

} // namespace editor
