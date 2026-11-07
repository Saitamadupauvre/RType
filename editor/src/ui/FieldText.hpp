#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "engine/core/ecs/FieldRegistry.hpp"

namespace editor {

/** @return `#RRGGBBAA` in uppercase hexadecimal. */
[[nodiscard]] std::string color_to_hex(engine::core::Rgba color);

/**
 * @return The color written `#RRGGBB` or `#RRGGBBAA`, in either case, `#` optional; an omitted
 * alpha is opaque. std::nullopt for anything else.
 */
[[nodiscard]] std::optional<engine::core::Rgba> color_from_hex(std::string_view text);

/** @return A one-line description of a value the inspector cannot edit yet, such as a list. */
[[nodiscard]] std::string describe(const engine::core::FieldValue& value);

} // namespace editor
