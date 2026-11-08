#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "../GameFixture.hpp"
#include "../play/FakeLauncher.hpp"
#include "action/Editor.hpp"
#include "action/EditorState.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/SceneDocument.hpp"

using editor::ConsoleLine;
using editor::Editor;
using editor::Mode;
using editor::Severity;
using engine::core::Vec2;
using engine::editor::InstanceId;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

class EditorTest : public GameFixture {
protected:
    Editor _editor{open_level(), _game.path() / ".scratch"};
    InstanceId _rock = id_of(_editor.session(), "rock");

    void SetUp() override {
        _editor.attach_launcher(std::make_unique<FakeLauncher>(std::make_shared<FakeGame>()));
    }

    [[nodiscard]] Vec2 rock_position() const { return instance(_editor.session(), _rock).position; }
};

} // namespace

TEST_F(EditorTest, EditsReachTheSessionInEditMode) {
    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});

    EXPECT_EQ(rock_position(), Vec2(9, 9));
    EXPECT_TRUE(_editor.session().unsaved());
    EXPECT_TRUE(_editor.state().console.empty());
}

TEST_F(EditorTest, EditsAreRefusedWhilePlaying) {
    _editor.handle(command::Play{});

    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});

    EXPECT_EQ(rock_position(), Vec2(5, 6));
    EXPECT_FALSE(_editor.session().unsaved());
    EXPECT_EQ(_editor.state().console,
              (std::vector<ConsoleLine>{
                  {.severity = Severity::error, .text = "Stop the game to change the project."}}));
}

TEST_F(EditorTest, EditsAreRefusedWhilePaused) {
    _editor.handle(command::Play{});
    _editor.handle(command::Pause{});

    _editor.handle(edit::Remove{.id = _rock});

    EXPECT_NE(_editor.session().document().find(_rock), nullptr);
}

TEST_F(EditorTest, EditsResumeAfterStop) {
    _editor.handle(command::Play{});
    _editor.handle(command::Stop{});

    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});

    EXPECT_EQ(_editor.state().mode, Mode::editing);
    EXPECT_EQ(rock_position(), Vec2(9, 9));
}

TEST_F(EditorTest, CommandsStillRunWhilePlaying) {
    _editor.handle(command::Play{});

    _editor.handle(command::Select{.id = _rock});
    _editor.handle(command::SetSnap{.enabled = false});

    EXPECT_EQ(_editor.session().selection(), _rock);
    EXPECT_FALSE(_editor.state().view.snap);
}

TEST_F(EditorTest, RefusedCommandsGoToTheConsole) {
    _editor.handle(command::Undo{});
    _editor.handle(edit::Remove{.id = InstanceId{.value = 999}});

    ASSERT_EQ(_editor.state().console.size(), 2U);
    for (const ConsoleLine& line : _editor.state().console) {
        EXPECT_EQ(line.severity, Severity::error);
        EXPECT_FALSE(line.text.empty());
    }
}

TEST_F(EditorTest, NotesOfSuccessfulCommandsGoToTheConsole) {
    _editor.handle(command::OpenScene{.name = "level1"});

    ASSERT_EQ(_editor.state().console.size(), 1U);
    EXPECT_EQ(_editor.state().console.front().severity, Severity::info);
}

TEST_F(EditorTest, SceneListFollowsSceneCommands) {
    _editor.handle(command::DuplicateScene{.from = "level1", .to = "copy"});

    EXPECT_EQ(_editor.state().scenes,
              (std::vector<std::string>{"broken", "copy", "faulty", "level1", "level2"}));
}

TEST_F(EditorTest, ProjectChangesAreRefusedWhileRunning) {
    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});
    _editor.handle(command::Play{});

    _editor.handle(command::Undo{});
    _editor.handle(command::DiscardChanges{});
    _editor.handle(command::OpenScene{.name = "level2"});
    _editor.handle(command::DuplicateScene{.from = "level1", .to = "copy"});
    _editor.handle(command::SetStartScene{.name = "level2"});

    EXPECT_EQ(rock_position(), Vec2(9, 9));
    EXPECT_EQ(_editor.session().open_scene_name(), "level1");
    EXPECT_EQ(_editor.session().start_scene(), "level1");
    EXPECT_EQ(_editor.state().scenes,
              (std::vector<std::string>{"broken", "faulty", "level1", "level2"}));
    EXPECT_EQ(_editor.state().console.size(), 5U);
}

TEST_F(EditorTest, ViewAndSaveStillWorkWhileRunning) {
    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});
    _editor.handle(command::Play{});

    _editor.handle(command::Save{});
    _editor.handle(command::SetTool{.tool = editor::Tool::pan});
    _editor.handle(command::Pause{});

    EXPECT_FALSE(_editor.session().unsaved());
    EXPECT_EQ(_editor.state().tool, editor::Tool::pan);
    EXPECT_EQ(_editor.state().mode, Mode::paused);
    EXPECT_TRUE(_editor.state().console.empty());
}

namespace {

namespace logging = engine::core::log;

class EditorLog : public EditorTest {
protected:
    std::shared_ptr<logging::MemorySink> _sink = std::make_shared<logging::MemorySink>();
    logging::ScopedSink _scope{_sink, logging::Level::Trace};

    [[nodiscard]] bool logged(logging::Level level, const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == level && record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(EditorLog, HandledActionsAreLoggedAtDebug) {
    _editor.handle(edit::Move{.id = _rock, .position = {9, 9}});

    EXPECT_TRUE(logged(logging::Level::Debug, "editor: edit::Move id="));
    EXPECT_FALSE(logged(logging::Level::Info, "refused"));
}

TEST_F(EditorLog, RefusedActionsAreLoggedAtInfoWithTheReason) {
    _editor.handle(command::Undo{});

    EXPECT_TRUE(logged(logging::Level::Debug, "editor: command::Undo"));
    EXPECT_TRUE(logged(logging::Level::Info, "editor: refused command::Undo: "));
}

TEST_F(EditorLog, SettingsSetTheViewAndReportProblems) {
    editor::LoadedSettings loaded;
    loaded.settings.view = {.snap = false, .grid_size = 8.F, .show_hitboxes = true};
    loaded.problems.push_back(
        {.severity = Severity::warning, .text = "settings.json: unknown key"});

    _editor.apply_settings(loaded);

    EXPECT_EQ(_editor.state().view, loaded.settings.view);
    ASSERT_FALSE(_editor.state().console.empty());
    EXPECT_EQ(_editor.state().console.back().severity, Severity::warning);
    EXPECT_TRUE(logged(logging::Level::Warn, "editor: settings.json: unknown key"));
}

TEST_F(EditorLog, ReloadSettingsAppliesTheFileNow) {
    _game.write("settings.json", R"({"ui.scale": 1.5})");
    _editor.apply_settings(editor::load_settings(_game.path() / "settings.json"));
    _game.write("settings.json",
                R"({"ui.scale": 2, "ui.scroll_speed": 0.5, "view.snap": false, "nope": 1})");

    _editor.handle(command::ReloadSettings{});

    EXPECT_EQ(_editor.state().ui_scale, 2.F);
    EXPECT_EQ(_editor.state().scroll_speed, 0.5F);
    EXPECT_FALSE(_editor.state().view.snap);
    EXPECT_TRUE(logged(logging::Level::Warn, "unknown key 'nope'"));
    ASSERT_FALSE(_editor.state().console.empty());
    EXPECT_NE(_editor.state().console.back().text.find("settings reloaded"), std::string::npos);
}

TEST_F(EditorLog, ReloadSettingsWorksWhilePlaying) {
    _game.write("settings.json", R"({"ui.scale": 1.25})");
    _editor.apply_settings(editor::load_settings(_game.path() / "settings.json"));
    _editor.handle(command::Play{});
    _game.write("settings.json", R"({"ui.scale": 2})");

    _editor.handle(command::ReloadSettings{});

    EXPECT_EQ(_editor.state().ui_scale, 2.F);
}

TEST_F(EditorLog, ReloadSettingsWithoutAFileIsRefused) {
    _editor.handle(command::ReloadSettings{});

    EXPECT_TRUE(logged(logging::Level::Info, "refused command::ReloadSettings"));
    EXPECT_EQ(_editor.state().ui_scale, 1.F);
}

TEST_F(EditorTest, ReloadScriptsShowsANewPrefab) {
    std::ofstream(_game.path() / "scripts" / "crate.lua") << "entity \"crate\"\n";

    _editor.handle(command::ReloadScripts{});

    EXPECT_EQ(_editor.state().prefabs, (std::vector<std::string>{"crate", "rock", "ship"}));
    ASSERT_EQ(_editor.state().console.size(), 1U);
    EXPECT_EQ(_editor.state().console.front().severity, Severity::info);
}

TEST_F(EditorTest, ReloadScriptsWorksWhilePlaying) {
    _editor.handle(command::Play{});
    std::ofstream(_game.path() / "scripts" / "crate.lua") << "entity \"crate\"\n";

    _editor.handle(command::ReloadScripts{});

    EXPECT_EQ(_editor.state().prefabs, (std::vector<std::string>{"crate", "rock", "ship"}));
}

TEST_F(EditorTest, ConsoleLinesNamingAScriptPointAtIt) {
    _editor.handle(command::ReportInvalidInput{.message = "lua: update: rock.lua:2: boom"});
    _editor.handle(command::ReportInvalidInput{.message = "lua: update: gone.lua:2: boom"});
    _editor.handle(command::ReportInvalidInput{.message = "not a number"});

    const auto& console = _editor.state().console;
    ASSERT_EQ(console.size(), 3U);
    EXPECT_EQ(console[0].script, _game.path() / "scripts" / "rock.lua");
    EXPECT_TRUE(console[1].script.empty());
    EXPECT_TRUE(console[2].script.empty());
}
