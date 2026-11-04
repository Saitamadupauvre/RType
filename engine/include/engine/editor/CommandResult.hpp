#pragma once

#include <string>
#include <utility>

namespace engine::editor {

/**
 * @brief Outcome of an EditorSession command.
 *
 * A refused command changes nothing and says why in @ref message, ready to show to the user.
 */
struct CommandResult {
    bool ok = true;
    /** Why the command was refused; empty or informative when it succeeded. */
    std::string message;

    /** @return A successful result, with an optional informative message. */
    [[nodiscard]] static CommandResult done(std::string note = {}) {
        return {.ok = true, .message = std::move(note)};
    }

    /** @return A refused result explaining why. */
    [[nodiscard]] static CommandResult refused(std::string reason) {
        return {.ok = false, .message = std::move(reason)};
    }

    bool operator==(const CommandResult&) const = default;
};

} // namespace engine::editor
