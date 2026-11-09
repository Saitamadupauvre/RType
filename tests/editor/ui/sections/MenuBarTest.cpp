#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/CommandTable.hpp"
#include "ui/UiInput.hpp"

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

enum Title : std::uint8_t { file, edit_menu, view, scene, help };

class MenuBar : public FrameFixture {
protected:
    static Clay_ElementId title(Title menu) { return CLAY_IDI("MenuTitle", menu); }

    static Clay_ElementId entry(std::string_view label) {
        const auto table = editor::command_table();
        const auto found = std::ranges::find(table, label, &editor::CommandEntry::label);
        EXPECT_NE(found, table.end()) << label;
        return CLAY_IDI("MenuEntry",
                        static_cast<std::uint32_t>(std::distance(table.begin(), found)));
    }

    void open(Title menu) {
        click(title(menu));
        frame();
    }

    void hover(Clay_ElementId id) {
        const Clay_BoundingBox box = box_of(id);
        frame({.pointer = {.x = box.x + (box.width / 2.F), .y = box.y + (box.height / 2.F)}});
        frame({.pointer = {.x = box.x + (box.width / 2.F), .y = box.y + (box.height / 2.F)}});
    }
};

} // namespace

TEST_F(MenuBar, MenusAreClosedAtFirst) { EXPECT_FALSE(shows("Save scene")); }

TEST_F(MenuBar, ClickingATitleOpensItsMenu) {
    open(file);

    EXPECT_TRUE(shows("Save scene"));
    EXPECT_FALSE(shows("Delete selection"));
}

TEST_F(MenuBar, ClickingTheOpenTitleClosesIt) {
    open(file);
    click(title(file));

    EXPECT_FALSE(shows("Save scene"));
}

TEST_F(MenuBar, EntriesShowTheirShortcut) {
    open(edit_menu);

    EXPECT_TRUE(shows(editor::shortcut_label({.key = editor::Key::z, .primary_modifier = true})));
    EXPECT_TRUE(shows("Del"));
}

TEST_F(MenuBar, ClickingAnEntryEmitsItsActionAndCloses) {
    open(view);
    const auto actions = click(entry("Show hitboxes"));

    const auto* overlay = only<command::SetOverlay>(actions);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->overlay, editor::Overlay::hitboxes);
    EXPECT_TRUE(overlay->visible);
    EXPECT_FALSE(shows("Show hitboxes"));
}

TEST_F(MenuBar, DisabledEntriesEmitNothing) {
    open(edit_menu);

    EXPECT_TRUE(click(entry("Undo")).empty());
    EXPECT_TRUE(shows("Undo"));
}

TEST_F(MenuBar, EntriesFollowTheEditor) {
    _editor.handle(edit::Move{.id = id_of(_editor.session(), "rock"), .position = {9, 9}});
    open(edit_menu);

    EXPECT_NE(only<command::Undo>(click(entry("Undo"))), nullptr);
}

TEST_F(MenuBar, TogglesShowACheckWhenOn) {
    open(view);
    const auto checks = std::ranges::count(frame().texts, "\xE2\x9C\x93");

    EXPECT_EQ(checks, 3);
}

TEST_F(MenuBar, HoveringAnotherTitleSwitchesTheOpenMenu) {
    open(file);
    hover(title(scene));

    EXPECT_TRUE(shows("Play"));
    EXPECT_FALSE(shows("Save scene"));
}

TEST_F(MenuBar, HoveringATitleWithNothingOpenOpensNothing) {
    frame();
    hover(title(scene));

    EXPECT_FALSE(shows("Pause"));
}

TEST_F(MenuBar, EscapeCloses) {
    open(file);
    frame({.keys_pressed = {editor::Key::escape}});

    EXPECT_FALSE(shows("Save scene"));
}

TEST_F(MenuBar, ClickingOutsideClosesWithoutEmitting) {
    open(file);
    const auto actions = click(CLAY_ID("StatusBar"));

    EXPECT_TRUE(actions.empty());
    EXPECT_FALSE(shows("Save scene"));
}

TEST_F(MenuBar, HelpListsTheViewportControls) {
    open(help);

    EXPECT_TRUE(shows("Pan the view"));
    EXPECT_TRUE(shows("Middle drag"));
}

TEST_F(MenuBar, ScenePlaysAndStops) {
    open(scene);
    EXPECT_NE(only<command::Play>(click(entry("Play"))), nullptr);
    EXPECT_TRUE(click(entry("Stop")).empty());
}
