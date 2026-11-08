#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/CommandTable.hpp"
#include "ui/UiInput.hpp"

using editor::Key;
using editor::NewItem;
using editor::UiInput;

namespace command = editor::command;

namespace {

class NameDialog : public FrameFixture {
protected:
    void open_dialog(NewItem item) {
        _editor.handle(command::OpenNameDialog{.item = item});
        frame();
    }

    std::vector<editor::Action> step(const UiInput& input) {
        std::vector<editor::Action> actions = frame(input).actions;
        for (const editor::Action& action : actions) {
            _editor.handle(action);
        }
        return actions;
    }

    void type(const std::string& text) {
        step({.typed = text});
        frame();
    }

    template <typename Wanted>
    static const Wanted* find(const std::vector<editor::Action>& actions) {
        for (const editor::Action& action : actions) {
            if (const auto* known = std::get_if<editor::Command>(&action)) {
                if (const auto* wanted = std::get_if<Wanted>(known)) {
                    return wanted;
                }
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool is_open() const { return _editor.state().name_dialog.has_value(); }

    static const editor::CommandEntry& entry(std::string_view label) {
        const auto table = editor::command_table();
        const auto found = std::ranges::find(table, label, &editor::CommandEntry::label);
        EXPECT_NE(found, table.end()) << label;
        return *found;
    }
};

} // namespace

TEST_F(NameDialog, HiddenUntilOpened) {
    EXPECT_FALSE(shows("NEW PREFAB"));
    EXPECT_FALSE(shows("NEW SCENE"));
}

TEST_F(NameDialog, OpensWithItsFieldFocused) {
    open_dialog(NewItem::prefab);

    EXPECT_TRUE(shows("NEW PREFAB"));
    frame();
    EXPECT_TRUE(_widgets.is_focused(CLAY_ID("NameDialogField")));
}

TEST_F(NameDialog, TypingKeepsTheName) {
    open_dialog(NewItem::prefab);

    type("crate");

    ASSERT_TRUE(is_open());
    EXPECT_EQ(_editor.state().name_dialog->name, "crate");
}

TEST_F(NameDialog, EnterCreatesThePrefab) {
    open_dialog(NewItem::prefab);
    type("crate");

    const auto actions = step({.keys_pressed = {Key::enter}});

    const auto* create = find<command::CreatePrefab>(actions);
    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->name, "crate");
    EXPECT_FALSE(is_open());
}

TEST_F(NameDialog, EnterCreatesTheScene) {
    open_dialog(NewItem::scene);
    type("boss");

    const auto actions = step({.keys_pressed = {Key::enter}});

    const auto* create = find<command::CreateScene>(actions);
    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->name, "boss");
    EXPECT_EQ(_editor.session().open_scene_name(), "boss");
}

TEST_F(NameDialog, CreateButtonCreates) {
    open_dialog(NewItem::scene);
    type("boss");

    const auto actions = click(CLAY_ID("NameDialogCreate"));

    const auto* create = find<command::CreateScene>(actions);
    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->name, "boss");
}

TEST_F(NameDialog, ARefusedNameKeepsTheDialogOpen) {
    open_dialog(NewItem::scene);
    type("level1");

    step({.keys_pressed = {Key::enter}});
    frame();

    ASSERT_TRUE(is_open());
    EXPECT_EQ(_editor.state().name_dialog->name, "level1");
    EXPECT_TRUE(_widgets.is_focused(CLAY_ID("NameDialogField")));
}

TEST_F(NameDialog, EscapeCancelAndAClickOutsideClose) {
    open_dialog(NewItem::prefab);
    EXPECT_NE(find<command::CloseNameDialog>(step({.keys_pressed = {Key::escape}})), nullptr);

    open_dialog(NewItem::prefab);
    frame();
    EXPECT_NE(find<command::CloseNameDialog>(click(CLAY_ID("NameDialogCancel"))), nullptr);

    open_dialog(NewItem::prefab);
    frame();
    EXPECT_NE(find<command::CloseNameDialog>(step(
                  {.pointer = {.x = 4, .y = 4}, .primary_down = true, .primary_pressed = true})),
              nullptr);
}

TEST_F(NameDialog, ShortcutsAndThePaletteWaitWhileItIsOpen) {
    open_dialog(NewItem::prefab);

    EXPECT_EQ(
        find<command::Save>(frame({.keys_pressed = {Key::s}, .primary_modifier = true}).actions),
        nullptr);
    frame({.keys_pressed = {Key::p}, .primary_modifier = true, .shift = true});
    EXPECT_FALSE(_widgets.palette().open);
}

TEST_F(NameDialog, MenuEntriesOpenItForPrefabsAndScenes) {
    const auto prefab = entry("New prefab...").make(_editor);
    const auto scene = entry("New scene...").make(_editor);

    ASSERT_TRUE(prefab.has_value());
    ASSERT_TRUE(scene.has_value());
    const auto* open_prefab =
        std::get_if<command::OpenNameDialog>(&std::get<editor::Command>(*prefab));
    const auto* open_scene =
        std::get_if<command::OpenNameDialog>(&std::get<editor::Command>(*scene));
    ASSERT_NE(open_prefab, nullptr);
    ASSERT_NE(open_scene, nullptr);
    EXPECT_EQ(open_prefab->item, NewItem::prefab);
    EXPECT_EQ(open_scene->item, NewItem::scene);
}

TEST_F(NameDialog, MenuEntriesAreDisabledWhilePlayingOrOpen) {
    open_dialog(NewItem::prefab);
    EXPECT_FALSE(entry("New scene...").make(_editor).has_value());
    _editor.handle(command::CloseNameDialog{});

    _editor.handle(command::Play{});
    EXPECT_FALSE(entry("New prefab...").make(_editor).has_value());
}
