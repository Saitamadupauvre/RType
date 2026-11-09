#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "FrameFixture.hpp"
#include "action/Action.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
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
    std::vector<Shortcut> seen{editor::palette_shortcut};
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

TEST_F(CommandTable, CtrlDDuplicatesTheSelectionOneGridCellAway) {
    EXPECT_TRUE(press(Key::d, true).empty());

    const auto rock = id_of(_editor.session(), "rock");
    _editor.handle(command::Select{.id = rock});
    _editor.handle(command::SetGridSize{.size = 32.F});
    const auto actions = press(Key::d, true);

    ASSERT_EQ(actions.size(), 1U);
    const auto* edit = std::get_if<editor::Edit>(&actions.front());
    ASSERT_NE(edit, nullptr);
    const auto* duplicate = std::get_if<edit::Duplicate>(edit);
    ASSERT_NE(duplicate, nullptr);
    EXPECT_EQ(duplicate->id, rock);
    EXPECT_EQ(duplicate->offset, (engine::core::Vec2{32, 32}));
    EXPECT_TRUE(press(Key::d).empty());
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

namespace {

namespace logging = engine::core::log;

class ShortcutLog : public CommandTable {
protected:
    std::shared_ptr<logging::MemorySink> _sink = std::make_shared<logging::MemorySink>();
    logging::ScopedSink _scope{_sink, logging::Level::Trace};

    [[nodiscard]] bool traced(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logging::Level::Trace &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(ShortcutLog, KeysAndTheShortcutTheyRunAreTraced) {
    press(Key::g);

    EXPECT_TRUE(traced("editor: key G"));
    EXPECT_TRUE(traced("editor: shortcut G runs Move tool"));
}

TEST_F(ShortcutLog, DisabledShortcutsAreTraced) {
    press(Key::z, true);

    EXPECT_TRUE(traced("editor: shortcut " +
                       editor::shortcut_label({.key = Key::z, .primary_modifier = true}) +
                       " skipped, Undo is disabled"));
}

TEST_F(ShortcutLog, IgnoredShortcutsAreTraced) {
    _widgets.focus(CLAY_ID("SomeField"), "text");
    press(Key::v);

    EXPECT_TRUE(traced("shortcuts ignored"));
}

TEST_F(ShortcutLog, NothingIsTracedAboveTheLevel) {
    logging::set_level(logging::Level::Debug);
    press(Key::g);

    EXPECT_FALSE(traced("editor: key"));
}

TEST_F(CommandTable, ZoomStepsTheUiScaleByATenth) {
    const auto table = command_table();
    const auto entry = [&](std::string_view label) {
        return *std::ranges::find(table, label, &editor::CommandEntry::label);
    };
    const auto scale_of = [](const std::optional<editor::Action>& action) {
        const auto* zoom = std::get_if<command::SetUiScale>(&std::get<editor::Command>(*action));
        return zoom == nullptr ? 0.F : zoom->scale;
    };

    EXPECT_FLOAT_EQ(scale_of(entry("Zoom in").make(_editor)), 1.1F);
    EXPECT_FLOAT_EQ(scale_of(entry("Zoom out").make(_editor)), 0.9F);

    _editor.handle(command::SetUiScale{.scale = 3.F});
    EXPECT_FALSE(entry("Zoom in").make(_editor).has_value());
    _editor.handle(command::SetUiScale{.scale = 0.5F});
    EXPECT_FALSE(entry("Zoom out").make(_editor).has_value());
}

TEST_F(CommandTable, SetUiScaleRefusesOutOfRangeValues) {
    _editor.handle(command::SetUiScale{.scale = 1.5F});
    _editor.handle(command::SetUiScale{.scale = 9.F});
    _editor.handle(command::SetUiScale{.scale = 0.1F});

    EXPECT_FLOAT_EQ(_editor.state().ui_scale, 1.5F);
    EXPECT_EQ(_editor.state().refusal.count, 2U);
}

TEST_F(CommandTable, CtrlPlusAndMinusZoomWithOrWithoutShift) {
    const auto zoom_of = [](const std::vector<editor::Action>& actions) {
        const auto* zoom = only<command::SetUiScale>(actions);
        return zoom == nullptr ? 0.F : zoom->scale;
    };

    EXPECT_FLOAT_EQ(zoom_of(press(Key::plus, true)), 1.1F);
    EXPECT_FLOAT_EQ(zoom_of(press(Key::plus, true, true)), 1.1F);
    EXPECT_FLOAT_EQ(zoom_of(press(Key::minus, true)), 0.9F);
    EXPECT_TRUE(press(Key::plus).empty());
    EXPECT_TRUE(press(Key::minus).empty());
}

TEST(CommandTableEntries, ZoomShortcutsReadAsEqualsAndMinus) {
    EXPECT_EQ(editor::shortcut_label({.key = Key::plus, .primary_modifier = true}).back(), '=');
    EXPECT_EQ(editor::shortcut_label({.key = Key::minus, .primary_modifier = true}).back(), '-');
}
