#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>

namespace engine::script {

/**
 * @brief Actions given to one entity for a tick, read from Lua through `self.input`.
 *
 * `self.input:axis(name)` returns the value stored in axes, or 0 when absent.
 * `self.input:pressed(name)` returns true when the name is in pressed.
 *
 * @note The engine does not interpret the names: the game decides them (e.g. "move_x", "fire").
 */
struct InputState {
    std::map<std::string, float, std::less<>> axes;
    std::set<std::string, std::less<>> pressed;
};

} // namespace engine::script
