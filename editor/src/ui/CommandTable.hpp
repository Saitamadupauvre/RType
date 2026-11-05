#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "action/Action.hpp"
#include "action/Editor.hpp"
#include "ui/Ui.hpp"
#include "ui/UiInput.hpp"

namespace editor {

struct Shortcut {
    Key key = Key::s;
    bool primary_modifier = false;
    bool shift = false;

    bool operator==(const Shortcut&) const = default;
};

/**
 * @brief One user-facing command, shared by keyboard shortcuts, menus and the command palette.
 *
 * `make` returns the action for the current editor, or std::nullopt when the command is
 * disabled (nothing to undo, no selection...). A disabled command is never emitted.
 */
struct CommandEntry {
    std::string_view label;
    std::optional<Shortcut> shortcut;
    std::optional<Action> (*make)(const Editor& editor);
};

/** @brief Every command, in menu order. Shortcuts are unique across the table. */
[[nodiscard]] std::span<const CommandEntry> command_table();

/**
 * @brief Emits the action of each enabled command whose shortcut was pressed this frame.
 * @note Emits nothing while a field has the keyboard.
 */
void emit_shortcuts(Ui& ui, const Editor& editor);

} // namespace editor
