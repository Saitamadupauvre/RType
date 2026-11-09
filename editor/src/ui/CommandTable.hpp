#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
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
    /** Matches with or without Shift, for keys whose shifted symbol is the same command (`=`, `+`).
     */
    bool any_shift = false;

    bool operator==(const Shortcut&) const = default;
};

/** @brief The menu bar menu a command is listed in. */
enum class Menu : std::uint8_t { file, edit, view, scene };

/** @brief The title of @p menu in the menu bar. */
[[nodiscard]] constexpr std::string_view menu_title(Menu menu) {
    switch (menu) {
    case Menu::file:
        return "File";
    case Menu::edit:
        return "Edit";
    case Menu::view:
        return "View";
    case Menu::scene:
        return "Scene";
    }
    return "";
}

/** @brief Opens the command palette; never used by an entry of the table. */
inline constexpr Shortcut palette_shortcut{.key = Key::p, .primary_modifier = true, .shift = true};

/**
 * @brief One user-facing command, shared by keyboard shortcuts, menus and the command palette.
 *
 * `make` returns the action for the current editor, or std::nullopt when the command is
 * disabled (nothing to undo, no selection...). A disabled command is never emitted.
 * `checked`, when set, makes the command a toggle and tells whether it is on.
 */
struct CommandEntry {
    std::string_view label;
    Menu menu = Menu::file;
    std::optional<Shortcut> shortcut;
    std::optional<Action> (*make)(const Editor& editor);
    bool (*checked)(const Editor& editor) = nullptr;
};

/** @brief Every command, in menu order. Shortcuts are unique across the table. */
[[nodiscard]] std::span<const CommandEntry> command_table();

/** @brief The shortcut as menus print it, such as "Ctrl+S" ("Cmd+S" on macOS) or "Del". */
[[nodiscard]] std::string shortcut_label(const Shortcut& shortcut);

/**
 * @brief Emits the action of each enabled command whose shortcut was pressed this frame.
 * @note Emits nothing while a field has the keyboard or a dialog (export, name) is open.
 */
void emit_shortcuts(Ui& ui, const Editor& editor);

} // namespace editor
