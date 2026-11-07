#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/CommandTable.hpp"
#include "ui/UiInput.hpp"

using editor::command_table;
using editor::Key;
using editor::Shortcut;
using editor::Tool;
using editor::UiInput;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

class CommandTable : public FrameFixture {
protected:
    std::vector<editor::Action> press(Key key, bool primary = false, bool shift = false) {
        return frame(UiInput{.keys_pressed = {key}, .primary_modifier = primary, .shift = shift})
            .actions;
    }

    void move_rock() {
        _editor.handle(edit::Move{.id = id_of(_editor.session(), "rock"), .position = {9, 9}});
    }
};

} // namespace

TEST(CommandTableEntries, ShortcutsAreUnique) {
    std::vector<Shortcut> seen;
    for (const auto& entry : command_table()) {
        if (entry.shortcut) {
            EXPECT_EQ(std::ranges::count(seen, *entry.shortcut), 0) << entry.label;
            seen.push_back(*entry.shortcut);
        }
    }
}

TEST_F(CommandTable, CtrlSSaves) {
    const auto actions = press(Key::s, true);
    EXPECT_NE(only<command::Save>(actions), nullptr);
}

TEST_F(CommandTable, KeyWithoutItsModifierDoesNothing) {
    EXPECT_TRUE(press(Key::s).empty());
    EXPECT_TRUE(press(Key::s, true, true).empty());
}

TEST_F(CommandTable, ModifierOnPlainShortcutDoesNothing) {
    EXPECT_TRUE(press(Key::v, true).empty());
}

TEST_F(CommandTable, UndoAndRedoFollowTheHistory) {
    EXPECT_TRUE(press(Key::z, true).empty());

    move_rock();
    const auto undo = press(Key::z, true);
    EXPECT_NE(only<command::Undo>(undo), nullptr);

    _editor.handle(command::Undo{});
    const auto redo = press(Key::y, true);
    EXPECT_NE(only<command::Redo>(redo), nullptr);
}

TEST_F(CommandTable, UndoIsDisabledWhilePlaying) {
    move_rock();
    _editor.handle(command::Play{});

    EXPECT_TRUE(press(Key::z, true).empty());
}

TEST_F(CommandTable, DeleteRemovesTheSelection) {
    EXPECT_TRUE(press(Key::del).empty());

    const auto rock = id_of(_editor.session(), "rock");
    _editor.handle(command::Select{.id = rock});
    const auto actions = press(Key::del);

    ASSERT_EQ(actions.size(), 1U);
    const auto* edit = std::get_if<editor::Edit>(&actions.front());
    ASSERT_NE(edit, nullptr);
    const auto* remove = std::get_if<edit::Remove>(edit);
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->id, rock);
}

TEST_F(CommandTable, ToolKeysSwitchTools) {
    for (const auto [key, tool] : {std::pair{Key::v, Tool::select}, std::pair{Key::g, Tool::move},
                                   std::pair{Key::h, Tool::pan}}) {
        const auto actions = press(key);
        const auto* set = only<command::SetTool>(actions);
        ASSERT_NE(set, nullptr);
        EXPECT_EQ(set->tool, tool);
    }
}

TEST_F(CommandTable, SaveIsDisabledWithoutAnOpenScene) {
    _game.write("game.json", R"({"name": "editor-test"})");
    auto session = engine::editor::EditorSession::open(_game.path());
    ASSERT_TRUE(session.has_value());
    _editor = editor::Editor(std::move(*session));

    EXPECT_TRUE(press(Key::s, true).empty());
}

TEST_F(CommandTable, ShortcutsAreIgnoredWhileAFieldHasTheKeyboard) {
    _widgets.focus(CLAY_ID("SomeField"), "text");
    EXPECT_TRUE(press(Key::v).empty());
}

TEST(CommandTableEntries, ShortcutLabelsNameTheModifierAndTheKey) {
#if defined(__APPLE__)
    EXPECT_EQ(editor::shortcut_label({.key = Key::s, .primary_modifier = true}), "Cmd+S");
#else
    EXPECT_EQ(editor::shortcut_label({.key = Key::s, .primary_modifier = true}), "Ctrl+S");
#endif
    EXPECT_EQ(editor::shortcut_label({.key = Key::del}), "Del");
    EXPECT_EQ(editor::shortcut_label({.key = Key::z, .shift = true}), "Shift+Z");
}
