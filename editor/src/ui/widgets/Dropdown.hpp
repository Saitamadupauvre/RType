#pragma once

#include <clay.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "ui/Ui.hpp"

namespace editor {

/**
 * @brief Draws a button showing `options[selected]` that opens a list of every option.
 *
 * The list floats over the layout, unclipped by scroll containers. While it is open, Up and
 * Down move the highlight, Enter picks it, Esc or a click outside closes it.
 * @param options Must outlive the render of the frame.
 * @return The index picked this frame; the caller emits the change.
 */
[[nodiscard]] std::optional<std::size_t> dropdown(Ui& ui, Clay_ElementId id, std::size_t selected,
                                                  std::span<const std::string_view> options);

} // namespace editor
