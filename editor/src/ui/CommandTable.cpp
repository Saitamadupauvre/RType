#include "ui/CommandTable.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include "engine/core/log/Log.hpp"

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

std::optional<Action> discard(const Editor& editor) {
    if (!editing(editor) || !editor.session().unsaved()) {
        return std::nullopt;
    }
    return command::DiscardChanges{};
}

std::optional<Action> open_script(const Editor& editor) {
    const auto& inspector = editor.state().inspector;
    if (!inspector || inspector->script.empty()) {
        return std::nullopt;
    }
    return command::OpenScript{.file = inspector->script};
}

std::optional<Action> open_settings(const Editor& editor) {
    if (editor.state().settings_file.empty()) {
        return std::nullopt;
    }
    return command::OpenSettings{};
}

std::optional<Action> open_export(const Editor& editor) {
    if (!editing(editor) || editor.state().export_dialog) {
        return std::nullopt;
    }
    return command::OpenExport{};
}

template <Tool tool> std::optional<Action> use_tool(const Editor&) {
    return command::SetTool{.tool = tool};
}

template <Tool tool> bool uses_tool(const Editor& editor) { return editor.state().tool == tool; }

std::optional<Action> toggle_snap(const Editor& editor) {
    return command::SetSnap{.enabled = !editor.state().view.snap};
}

bool snaps(const Editor& editor) { return editor.state().view.snap; }

bool shows(const Editor& editor, Overlay overlay) {
    const ViewSettings& view = editor.state().view;
    return overlay == Overlay::hitboxes ? view.show_hitboxes : view.show_names;
}

template <Overlay overlay> std::optional<Action> toggle_overlay(const Editor& editor) {
    return command::SetOverlay{.overlay = overlay, .visible = !shows(editor, overlay)};
}

template <Overlay overlay> bool showing(const Editor& editor) { return shows(editor, overlay); }

std::optional<Action> play(const Editor& editor) {
    if (editor.state().mode == Mode::playing) {
        return std::nullopt;
    }
    return command::Play{};
}

std::optional<Action> pause(const Editor& editor) {
    if (editor.state().mode != Mode::playing) {
        return std::nullopt;
    }
    return command::Pause{};
}

std::optional<Action> stop(const Editor& editor) {
    if (editing(editor)) {
        return std::nullopt;
    }
    return command::Stop{};
}

constexpr Shortcut primary(Key key) { return {.key = key, .primary_modifier = true}; }

constexpr Shortcut plain(Key key) { return {.key = key}; }

constexpr std::array table{
    CommandEntry{.label = "Save scene", .menu = Menu::file, .shortcut = primary(Key::s), .make = save},
    CommandEntry{.label = "Discard changes", .menu = Menu::file, .shortcut = std::nullopt, .make = discard},
    CommandEntry{.label = "Open script", .menu = Menu::file, .shortcut = std::nullopt, .make = open_script},
    CommandEntry{.label = "Open settings",
                 .menu = Menu::file,
                 .shortcut = std::nullopt,
                 .make = open_settings},
    CommandEntry{.label = "Export game...",
                 .menu = Menu::file,
                 .shortcut = std::nullopt,
                 .make = open_export},
    CommandEntry{.label = "Undo", .menu = Menu::edit, .shortcut = primary(Key::z), .make = undo},
    CommandEntry{.label = "Redo", .menu = Menu::edit, .shortcut = primary(Key::y), .make = redo},
    CommandEntry{.label = "Delete selection",
                 .menu = Menu::edit,
                 .shortcut = plain(Key::del),
                 .make = delete_selection},
    CommandEntry{.label = "Select tool",
                 .menu = Menu::view,
                 .shortcut = plain(Key::v),
                 .make = use_tool<Tool::select>,
                 .checked = uses_tool<Tool::select>},
    CommandEntry{.label = "Move tool",
                 .menu = Menu::view,
                 .shortcut = plain(Key::g),
                 .make = use_tool<Tool::move>,
                 .checked = uses_tool<Tool::move>},
    CommandEntry{.label = "Pan tool",
                 .menu = Menu::view,
                 .shortcut = plain(Key::h),
                 .make = use_tool<Tool::pan>,
                 .checked = uses_tool<Tool::pan>},
    CommandEntry{
        .label = "Snap to grid", .menu = Menu::view, .shortcut = std::nullopt, .make = toggle_snap, .checked = snaps},
    CommandEntry{.label = "Show hitboxes",
                 .menu = Menu::view,
                 .shortcut = std::nullopt,
                 .make = toggle_overlay<Overlay::hitboxes>,
                 .checked = showing<Overlay::hitboxes>},
    CommandEntry{.label = "Show names",
                 .menu = Menu::view,
                 .shortcut = std::nullopt,
                 .make = toggle_overlay<Overlay::names>,
                 .checked = showing<Overlay::names>},
    CommandEntry{.label = "Play", .menu = Menu::scene, .shortcut = std::nullopt, .make = play},
    CommandEntry{.label = "Pause", .menu = Menu::scene, .shortcut = std::nullopt, .make = pause},
    CommandEntry{.label = "Stop", .menu = Menu::scene, .shortcut = std::nullopt, .make = stop},
};

#if defined(__APPLE__)
constexpr std::string_view primary_name = "Cmd";
#else
constexpr std::string_view primary_name = "Ctrl";
#endif

std::string_view key_name(Key key) {
    switch (key) {
    case Key::a:
        return "A";
    case Key::c:
        return "C";
    case Key::s:
        return "S";
    case Key::v:
        return "V";
    case Key::x:
        return "X";
    case Key::y:
        return "Y";
    case Key::z:
        return "Z";
    case Key::g:
        return "G";
    case Key::h:
        return "H";
    case Key::p:
        return "P";
    case Key::del:
        return "Del";
    case Key::backspace:
        return "Backspace";
    case Key::enter:
        return "Enter";
    case Key::escape:
        return "Esc";
    case Key::tab:
        return "Tab";
    case Key::left:
        return "Left";
    case Key::right:
        return "Right";
    case Key::up:
        return "Up";
    case Key::down:
        return "Down";
    case Key::home:
        return "Home";
    case Key::end:
        return "End";
    }
    return "";
}

bool pressed(const UiInput& input, const Shortcut& shortcut) {
    return input.primary_modifier == shortcut.primary_modifier && input.shift == shortcut.shift &&
           std::ranges::find(input.keys_pressed, shortcut.key) != input.keys_pressed.end();
}

} // namespace

std::span<const CommandEntry> command_table() { return table; }

std::string shortcut_label(const Shortcut& shortcut) {
    std::string label;
    if (shortcut.primary_modifier) {
        label.append(primary_name).append("+");
    }
    if (shortcut.shift) {
        label.append("Shift+");
    }
    return label.append(key_name(shortcut.key));
}

void trace_keys(const UiInput& input) {
    for (const Key key : input.keys_pressed) {
        engine::core::log::trace("editor: key {}", shortcut_label({.key = key,
                                                                    .primary_modifier =
                                                                        input.primary_modifier,
                                                                    .shift = input.shift}));
    }
}

void emit_shortcuts(Ui& ui, const Editor& editor) {
    namespace logging = engine::core::log;
    trace_keys(ui.input());
    if (ui.widgets().wants_keyboard() || editor.state().export_dialog) {
        if (!ui.input().keys_pressed.empty()) {
            logging::trace("editor: shortcuts ignored, a field or dialog has the keyboard");
        }
        return;
    }
    for (const CommandEntry& entry : table) {
        if (!entry.shortcut || !pressed(ui.input(), *entry.shortcut)) {
            continue;
        }
        if (auto action = entry.make(editor)) {
            logging::trace("editor: shortcut {} runs {}", shortcut_label(*entry.shortcut),
                           entry.label);
            ui.emit(std::move(*action));
        } else {
            logging::trace("editor: shortcut {} skipped, {} is disabled",
                           shortcut_label(*entry.shortcut), entry.label);
        }
    }
}

} // namespace editor
