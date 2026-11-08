#pragma once

#include <string>

#include "action/Action.hpp"

namespace editor {

/**
 * @brief Describes an action for the developer log: its name, then its fields.
 *
 * For example `edit::Move id=3 position=(9, 9)` or `command::Save`.
 */
[[nodiscard]] std::string describe_action(const Action& action);

} // namespace editor
