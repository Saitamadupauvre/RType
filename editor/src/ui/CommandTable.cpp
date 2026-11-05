#include "ui/CommandTable.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace editor {

namespace {

bool editing(const Editor& editor) { return editor.state().mode == Mode::editing; }

std::optional<Action> save(const Editor& editor) {
    if (!editor.session().open_scene_name()) {
        return std::nullopt;
    }
    return command::Save{};
}

std::optional<Action> undo(const Editor& editor) {
    if (!editing(editor) || !editor.session().can_undo()) {
        return std::nullopt;
    }
    return command::Undo{};
}

std::optional<Action> redo(const Editor& editor) {
    if (!editing(editor) || !editor.session().can_redo()) {
        return std::nullopt;
    }
    return command::Redo{};
}

std::optional<Action> delete_selection(const Editor& editor) {
    const auto selection = editor.session().selection();
    if (!editing(editor) || !selection) {
        return std::nullopt;
    }
    return edit::Remove{.id = *selection};
}

template <Tool tool> std::optional<Action> use_tool(const Editor&) {
    return command::SetTool{.tool = tool};
}

constexpr Shortcut primary(Key key) { return {.key = key, .primary_modifier = true}; }

constexpr Shortcut plain(Key key) { return {.key = key}; }

constexpr std::array table{
    CommandEntry{.label = "Save scene", .shortcut = primary(Key::s), .make = save},
    CommandEntry{.label = "Undo", .shortcut = primary(Key::z), .make = undo},
    CommandEntry{.label = "Redo", .shortcut = primary(Key::y), .make = redo},
    CommandEntry{
        .label = "Delete selection", .shortcut = plain(Key::del), .make = delete_selection},
    CommandEntry{.label = "Select tool", .shortcut = plain(Key::v), .make = use_tool<Tool::select>},
    CommandEntry{.label = "Move tool", .shortcut = plain(Key::g), .make = use_tool<Tool::move>},
    CommandEntry{.label = "Pan tool", .shortcut = plain(Key::h), .make = use_tool<Tool::pan>},
};

bool pressed(const UiInput& input, const Shortcut& shortcut) {
    return input.primary_modifier == shortcut.primary_modifier && input.shift == shortcut.shift &&
           std::ranges::find(input.keys_pressed, shortcut.key) != input.keys_pressed.end();
}

} // namespace

std::span<const CommandEntry> command_table() { return table; }

void emit_shortcuts(Ui& ui, const Editor& editor) {
    if (ui.widgets().wants_keyboard()) {
        return;
    }
    for (const CommandEntry& entry : table) {
        if (!entry.shortcut || !pressed(ui.input(), *entry.shortcut)) {
            continue;
        }
        if (auto action = entry.make(editor)) {
            ui.emit(std::move(*action));
        }
    }
}

} // namespace editor
