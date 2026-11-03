#pragma once

#include <optional>
#include <string>
#include <variant>

#include "engine/core/FieldRegistry.hpp"

namespace engine::script {

/**
 * @brief A Lua free field of an entity declaration that instances can override, such as
 * `health = 3` in `entity "boss" { health = 3 }`.
 *
 * Listed by ScriptRuntime::script_fields() and stored in instances under the key
 * `"Script.<name>"`. The kind is inferred from the default:
 * - a Lua integer is `int`, any other number `float` (`3` is an int, `3.0` a float);
 * - a boolean is `bool`, a string `string`;
 * - `{ x = .., y = .. }` is `vec2`, `{ r = .., g = .., b = .. }` with an optional `a` is `color`;
 * - a non-empty array of one of those kinds is a `list` (integers and floats together make a list
 *   of floats).
 *
 * Fields whose name starts with `_`, functions, other tables, and fields only set inside a
 * `server = { ... }` or `client = { ... }` section are not script fields.
 */
struct ScriptField {
    /** Name of the field in the declaration, without the `Script.` prefix. */
    std::string name;
    /** Value the declaration gives the field, of the field's kind. */
    core::FieldValue default_value;

    /** @return The kind every value of this field has. */
    [[nodiscard]] core::FieldKind kind() const noexcept { return core::kind_of(default_value); }

    /** @return The kind of the list items, or std::nullopt when the field is not a list. */
    [[nodiscard]] std::optional<core::FieldKind> element() const noexcept {
        if (const auto* list = std::get_if<core::FieldList>(&default_value)) {
            return list->element;
        }
        return std::nullopt;
    }

    bool operator==(const ScriptField&) const = default;
};

} // namespace engine::script
