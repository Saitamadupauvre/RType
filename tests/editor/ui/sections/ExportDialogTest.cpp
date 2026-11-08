#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "ui/UiInput.hpp"

namespace command = editor::command;

namespace {

class ExportDialog : public FrameFixture {
protected:
    void open_dialog() {
        _editor.handle(command::OpenExport{});
        frame();
    }

    void attach_runtime() {
        _game.write(".runtime/engine-runtime", "runtime");
        _game.write(".runtime/plugins/graphics-raylib.so", "graphics");
        _editor.attach_runtime(_game.path() / ".runtime");
    }
};

} // namespace

TEST_F(ExportDialog, ExportButtonOpensTheDialog) {
    EXPECT_NE(only<command::OpenExport>(click(CLAY_ID("ExportButton"))), nullptr);
}

TEST_F(ExportDialog, ExportButtonIsDisabledWhilePlaying) {
    _editor.handle(command::Play{});

    EXPECT_TRUE(click(CLAY_ID("ExportButton")).empty());
}

TEST_F(ExportDialog, HiddenUntilOpened) { EXPECT_FALSE(shows("EXPORT GAME")); }

TEST_F(ExportDialog, ShowsTheSettings) {
    open_dialog();

    EXPECT_TRUE(shows("EXPORT GAME"));
    EXPECT_TRUE(shows("editor-test"));
    EXPECT_TRUE(shows("Modes"));
    EXPECT_TRUE(shows("Server"));
}

TEST_F(ExportDialog, TurningOffTheServerModeChangesOnlyThatMode) {
    open_dialog();

    const auto actions = click(CLAY_ID("ExportModeDedicated"));
    const auto* change = only<command::SetExport>(actions);

    ASSERT_NE(change, nullptr);
    EXPECT_EQ(
        change->settings.modes,
        (editor::ExportModes{.solo = true, .host = true, .client = true, .dedicated = false}));
    EXPECT_EQ(change->settings.name, "editor-test");
}

TEST_F(ExportDialog, TurningAModeBackOnOffersItAgain) {
    open_dialog();
    auto settings = _editor.state().export_dialog->settings;
    settings.modes.solo = false;
    _editor.handle(command::SetExport{.settings = settings});
    frame();

    const auto* change = only<command::SetExport>(click(CLAY_ID("ExportModeSolo")));

    ASSERT_NE(change, nullptr);
    EXPECT_TRUE(change->settings.modes.solo);
}

TEST_F(ExportDialog, ExportIsDisabledWhileThePlanHasProblems) {
    open_dialog();

    EXPECT_TRUE(click(CLAY_ID("ExportRun")).empty());
}

TEST_F(ExportDialog, ExportEmitsRunWhenThePlanIsClean) {
    attach_runtime();
    open_dialog();

    EXPECT_TRUE(shows("and a README.txt telling how to run every mode."));
    EXPECT_NE(only<command::RunExport>(click(CLAY_ID("ExportRun"))), nullptr);
}

TEST_F(ExportDialog, CancelAndEscapeClose) {
    open_dialog();

    EXPECT_NE(only<command::CloseExport>(click(CLAY_ID("ExportCancel"))), nullptr);
    EXPECT_NE(only<command::CloseExport>(frame({.keys_pressed = {editor::Key::escape}}).actions),
              nullptr);
}

TEST_F(ExportDialog, ClicksDoNotReachTheWindowBehind) {
    open_dialog();

    EXPECT_TRUE(click(CLAY_ID("Viewport")).empty());
}

TEST_F(ExportDialog, ShortcutsAreIgnoredWhileOpen) {
    _editor.handle(command::Select{.id = id_of(_editor.session(), "rock")});
    open_dialog();

    EXPECT_TRUE(frame({.keys_pressed = {editor::Key::del}}).actions.empty());
}

TEST_F(ExportDialog, FieldTextIsDrawnAboveTheDialog) {
    open_dialog();
    const editor::Theme theme;
    const editor::UiInput input;
    editor::Ui ui(theme, input, _widgets, _platform);
    Clay_RenderCommandArray commands = editor::build_frame(ui, _editor, window);

    std::int32_t dialog = -1;
    std::int32_t name = -1;
    for (std::int32_t index = 0; index < commands.length; ++index) {
        const Clay_RenderCommand& command = *Clay_RenderCommandArray_Get(&commands, index);
        if (command.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE &&
            command.id == CLAY_ID("ExportDialog").id) {
            dialog = index;
        }
        if (command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
            const Clay_StringSlice slice = command.renderData.text.stringContents;
            if (std::string_view(slice.chars, static_cast<std::size_t>(slice.length)) ==
                "editor-test") {
                name = index;
            }
        }
    }

    ASSERT_GE(dialog, 0);
    ASSERT_GE(name, 0);
    EXPECT_GT(name, dialog);
}
