#include <gtest/gtest.h>

#include <filesystem>
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
        _game.write(".runtime/r-type_client", "client");
        _game.write(".runtime/r-type_server", "server");
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
    EXPECT_TRUE(shows("Client + server"));
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

TEST_F(ExportDialog, ChoosingClientOnlyChangesTheSettings) {
    open_dialog();

    const auto actions = click(CLAY_ID("ExportClientOnly"));
    const auto* change = only<command::SetExport>(actions);

    ASSERT_NE(change, nullptr);
    EXPECT_EQ(change->settings.binaries, editor::Binaries::client_only);
    EXPECT_EQ(change->settings.name, "editor-test");
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
