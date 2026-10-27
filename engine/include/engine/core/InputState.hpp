#pragma once

#include <optional>
#include <set>

#include "engine/core/Keys.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::core {

/**
 * @brief Raw keyboard and mouse state of one player for a tick, read from Lua through
 * `self.input`.
 *
 * `self.input:key(name)` is true while the key named @p name is held, `self.input:mouse(name)`
 * while the mouse button is held, and `self.input:pointer()` returns the pointer's x and y, or
 * nil when there is none. Names are those of the `keys` and `mouse_buttons` tables.
 *
 * @note The engine gives keys no meaning: the game's scripts decide what each one does.
 */
struct InputState {
    /** @brief Keys held. Key::Unknown is never held. */
    std::set<Key> keys;
    /** @brief Mouse buttons held. */
    std::set<MouseButton> mouse_buttons;
    /** @brief Pointer position in virtual units, such as the mouse, when one is known. */
    std::optional<Vec2> pointer;

    bool operator==(const InputState&) const = default;
};

} // namespace engine::core
