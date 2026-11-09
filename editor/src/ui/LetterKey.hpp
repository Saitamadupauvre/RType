#pragma once

#include <optional>
#include <string_view>

#include "ui/UiInput.hpp"

namespace editor {

/**
 * @brief The letter key a layout prints as `name`, such as "a" or "Z".
 * @return std::nullopt for anything that is not a letter the editor reacts to.
 */
[[nodiscard]] std::optional<Key> letter_key(std::string_view name);

} // namespace editor
