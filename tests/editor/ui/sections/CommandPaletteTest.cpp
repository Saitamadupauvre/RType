#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/CommandTable.hpp"
#include "ui/UiInput.hpp"

namespace command = editor::command;
using editor::Key;
using editor::UiInput;

namespace {

class CommandPalette : public FrameFixture {
protected:
    std::vector<editor::Action> press(Key key, bool primary = false, bool shift = false) {
        return frame(UiInput{.keys_pressed = {key}, .primary_modifier = primary, .shift = shift})
            .actions;
    }

    void open_palette() {
        press(Key::p, true, true);
        frame();
    }

    void type(const std::string& text) {
        frame(UiInput{.typed = text});
        frame();
    }

    [[nodiscard]] bool is_open() { return _widgets.palette().open; }

    static Clay_ElementId row(std::string_view label) {
        const auto table = editor::command_table();
        const auto found = std::ranges::find(table, label, &editor::CommandEntry::label);
        EXPECT_NE(found, table.end()) << label;
        return CLAY_IDI("PaletteRow",
                        static_cast<std::uint32_t>(std::distance(table.begin(), found)));
    }
};

} // namespace

TEST_F(CommandPalette, ClosedAtFirst) {
    EXPECT_FALSE(shows("Save scene"));
    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, CtrlShiftPOpensItWithEveryCommand) {
    open_palette();

    EXPECT_TRUE(is_open());
    EXPECT_TRUE(shows("Save scene"));
    EXPECT_TRUE(shows("Show hitboxes"));
    EXPECT_TRUE(_widgets.wants_keyboard());
}

TEST_F(CommandPalette, NeedsBothModifiers) {
    press(Key::p, true);
    press(Key::p, false, true);
    press(Key::p);

    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, TypingFiltersTheList) {
    open_palette();
    type("hit");

    EXPECT_TRUE(shows("Show hitboxes"));
    EXPECT_FALSE(shows("Save scene"));
}

TEST_F(CommandPalette, EnterRunsTheBestMatchAndCloses) {
    open_palette();
    type("hit");

    const auto actions = press(Key::enter);

    const auto* overlay = only<command::SetOverlay>(actions);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->overlay, editor::Overlay::hitboxes);
    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, DownMovesTheHighlight) {
    open_palette();
    type("show");
    press(Key::down);

    const auto actions = press(Key::enter);

    const auto* overlay = only<command::SetOverlay>(actions);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->overlay, editor::Overlay::names);
}

TEST_F(CommandPalette, UpStopsAtTheFirstRow) {
    open_palette();
    type("show");
    press(Key::up);

    const auto actions = press(Key::enter);

    const auto* overlay = only<command::SetOverlay>(actions);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(overlay->overlay, editor::Overlay::hitboxes);
}

TEST_F(CommandPalette, DisabledEntriesAreNeverRun) {
    open_palette();
    type("undo");

    EXPECT_TRUE(press(Key::enter).empty());
    EXPECT_TRUE(is_open());
    frame();
    EXPECT_TRUE(_widgets.wants_keyboard());
}

TEST_F(CommandPalette, ClickingARowRunsIt) {
    open_palette();
    type("names");

    const auto actions = click(row("Show names"));

    EXPECT_NE(only<command::SetOverlay>(actions), nullptr);
    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, EscapeCloses) {
    open_palette();

    EXPECT_TRUE(press(Key::escape).empty());
    EXPECT_FALSE(is_open());
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(CommandPalette, ClickingOutsideClosesWithoutRunning) {
    open_palette();

    EXPECT_TRUE(click(CLAY_ID("StatusBar")).empty());
    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, ShortcutsAreIgnoredWhileOpen) {
    _editor.handle(command::Select{.id = id_of(_editor.session(), "rock")});
    open_palette();

    EXPECT_TRUE(press(Key::del).empty());
    EXPECT_TRUE(press(Key::g).empty());
}

TEST_F(CommandPalette, NoMatchSaysSo) {
    open_palette();
    type("qqq");

    EXPECT_TRUE(shows("No command matches"));
    EXPECT_TRUE(press(Key::enter).empty());
}

TEST_F(CommandPalette, ExportDialogKeepsItClosed) {
    _editor.handle(command::OpenExport{});

    press(Key::p, true, true);

    EXPECT_FALSE(is_open());
}

TEST_F(CommandPalette, TabKeepsTheKeyboardInThePalette) {
    open_palette();
    press(Key::tab);
    frame();

    EXPECT_TRUE(is_open());
    EXPECT_TRUE(_widgets.is_focused(CLAY_ID("PaletteQuery")));
}

TEST_F(CommandPalette, ListsEveryCommandWhenTheWindowHasRoom) {
    open_palette();

    for (const auto& entry : editor::command_table()) {
        EXPECT_TRUE(shows(entry.label)) << entry.label;
    }
}

TEST_F(CommandPalette, ShortWindowsShowOnlyTheRowsThatFit) {
    constexpr Clay_Dimensions short_window{.width = 1440, .height = 300};
    frame(UiInput{.keys_pressed = {Key::p}, .primary_modifier = true, .shift = true}, {},
          short_window);
    const auto texts = frame({}, {}, short_window).texts;
    frame({}, {}, short_window);

    EXPECT_NE(std::ranges::find(texts, "Open script"), texts.end());
    EXPECT_EQ(std::ranges::find(texts, "Export game..."), texts.end());
    const Clay_BoundingBox last = box_of(row("Open script"));
    EXPECT_LE(last.y + last.height, short_window.height);
}

TEST_F(CommandPalette, OpensTheSettingsFile) {
    editor::LoadedSettings loaded;
    loaded.file = _game.path() / "settings.json";
    _editor.apply_settings(loaded);
    open_palette();
    type("settings");

    EXPECT_NE(only<command::OpenSettings>(press(Key::enter)), nullptr);
}

TEST_F(CommandPalette, OpenSettingsIsDisabledWithoutAConfigFolder) {
    open_palette();
    type("settings");

    EXPECT_TRUE(press(Key::enter).empty());
}

TEST_F(CommandPalette, ReloadsTheSettings) {
    editor::LoadedSettings loaded;
    loaded.file = _game.path() / "settings.json";
    _editor.apply_settings(loaded);
    open_palette();
    type("reload");

    EXPECT_NE(only<command::ReloadSettings>(press(Key::enter)), nullptr);
}
