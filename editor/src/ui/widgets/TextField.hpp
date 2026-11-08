#pragma once

#include <clay.h>

#include <cstdint>
#include <string_view>

#include "ui/Theme.hpp"
#include "ui/Ui.hpp"
#include "ui/widgets/FieldEvent.hpp"

namespace editor {

struct TextFieldOptions {
    std::string_view placeholder{};
    bool read_only = false;
    Face face = Face::regular;
    /** Z-index of the shown text; a field inside a floating element passes a layer above it. */
    std::int16_t layer = 0;
};

/**
 * @brief Draws a one-line text field showing `value`, edited as a draft while focused.
 *
 * A click focuses it on the next frame. Enter, Tab or a click outside commits the draft, Esc
 * restores `value`. A read-only field never takes focus.
 * @return The event of this frame; `value` carries the draft, the committed text, or the
 * restored text.
 */
[[nodiscard]] TextResult text_field(Ui& ui, Clay_ElementId id, std::string_view value,
                                    const TextFieldOptions& options = {});

} // namespace editor
