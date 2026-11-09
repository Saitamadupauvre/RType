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

/** @brief The part of a "Component.field" key before the dot; empty without a dot. */
[[nodiscard]] std::string_view component_of(std::string_view key);

/** @brief The field part of a key, as words: "PhysicsBody.resolve_collisions" is "resolve
 * collisions". */
[[nodiscard]] std::string field_label(std::string_view key);

} // namespace editor
