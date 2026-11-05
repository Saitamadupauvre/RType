#pragma once

#include <clay.h>

#include <optional>

#include "ui/Ui.hpp"
#include "ui/widgets/FieldEvent.hpp"

namespace editor {

struct NumberFieldOptions {
    std::optional<double> min;
    std::optional<double> max;
    int decimals = 2;
};

/**
 * @brief Draws a text field editing `value` as text; `decimals` applies to floats only.
 *
 * On commit the draft is parsed: a valid number is clamped to [min, max] and returned as
 * `committed`, anything else is reverted and returned as `cancelled` with `value`.
 * @return The event of this frame; `value` stays the input value until a valid commit.
 */
[[nodiscard]] FieldResult<float> number_field(Ui& ui, Clay_ElementId id, float value,
                                              const NumberFieldOptions& options = {});
/** @copydoc number_field(Ui&, Clay_ElementId, float, const NumberFieldOptions&) */
[[nodiscard]] FieldResult<int> number_field(Ui& ui, Clay_ElementId id, int value,
                                            const NumberFieldOptions& options = {});

} // namespace editor
