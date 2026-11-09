#include <gtest/gtest.h>

#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "../GameFixture.hpp"
#include "../open/FakeOpener.hpp"
#include "../play/FakeLauncher.hpp"
#include "action/ApplyCommand.hpp"
#include "action/ApplyEdit.hpp"
#include "action/EditorState.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/editor/EditorSession.hpp"

using editor::apply_command;
using editor::apply_edit;
using editor::EditorState;
using editor::Mode;
using editor::Overlay;
using engine::core::Vec2;
using engine::editor::EditorSession;
using engine::editor::InstanceId;

namespace command = editor::command;

namespace {

class ApplyCommand : public GameFixture {
protected:
    EditorSession _session = open_level();
    EditorState _state;
    editor::PlaySession _play{_game.path() / ".scratch"};
    std::shared_ptr<FakeOpenings> _openings = std::make_shared<FakeOpenings>();
    FakeOpener _opener{_openings};
    std::filesystem::path _runtime;
    InstanceId _rock = id_of(_session, "rock");

    void SetUp() override {
        _play.attach(std::make_unique<FakeLauncher>(std::make_shared<FakeGame>()));
    }

    bool run(const editor::Command& command) {
        return apply_command(_session, _state,
                             {.play = _play, .open_file = &_opener, .runtime = _runtime}, command)
            .ok;
    }

    void move_rock() {
        ASSERT_TRUE(apply_edit(_session, editor::edit::Move{.id = _rock, .position = {9, 9}}).ok);
    }
};

} // namespace

TEST_F(ApplyCommand, SelectReachesTheSession) {
    ASSERT_TRUE(run(command::Select{.id = _rock}));
    EXPECT_EQ(_session.selection(), _rock);

    ASSERT_TRUE(run(command::Select{.id = std::nullopt}));
    EXPECT_EQ(_session.selection(), std::nullopt);
}

TEST_F(ApplyCommand, UndoAndRedoWalkTheHistory) {
    move_rock();

    ASSERT_TRUE(run(command::Undo{}));
    EXPECT_EQ(instance(_session, _rock).position, Vec2(5, 6));

    ASSERT_TRUE(run(command::Redo{}));
    EXPECT_EQ(instance(_session, _rock).position, Vec2(9, 9));
}

TEST_F(ApplyCommand, UndoWithoutHistoryIsRefused) {
    EXPECT_FALSE(run(command::Undo{}));
    EXPECT_FALSE(run(command::Redo{}));
}

TEST_F(ApplyCommand, SaveClearsTheUnsavedFlag) {
    move_rock();

    ASSERT_TRUE(run(command::Save{}));

    EXPECT_FALSE(_session.unsaved());
}

TEST_F(ApplyCommand, DiscardChangesRestoresTheSavedDocument) {
    move_rock();

    ASSERT_TRUE(run(command::DiscardChanges{}));

    EXPECT_EQ(instance(_session, _rock).position, Vec2(5, 6));
}

TEST_F(ApplyCommand, SceneCommandsReachTheSession) {
    ASSERT_TRUE(run(command::DuplicateScene{.from = "level1", .to = "copy"}));
    ASSERT_TRUE(run(command::RenameScene{.from = "copy", .to = "renamed"}));
    ASSERT_TRUE(run(command::SetStartScene{.name = "renamed"}));
    EXPECT_EQ(_session.start_scene(), "renamed");

    ASSERT_TRUE(run(command::CreateScene{.name = "fresh"}));
    EXPECT_EQ(_session.open_scene_name(), "fresh");

    ASSERT_TRUE(run(command::OpenScene{.name = "level2"}));
    EXPECT_EQ(_session.open_scene_name(), "level2");

    ASSERT_TRUE(run(command::DeleteScene{.name = "fresh"}));
    EXPECT_EQ(_session.scenes(),
              (std::vector<std::string>{"broken", "faulty", "level1", "level2", "renamed"}));
}

TEST_F(ApplyCommand, OpeningOverUnsavedChangesIsRefused) {
    move_rock();

    EXPECT_FALSE(run(command::OpenScene{.name = "level2"}));
    EXPECT_EQ(_session.open_scene_name(), "level1");
}

TEST_F(ApplyCommand, ViewSettingsChangeTheState) {
    ASSERT_TRUE(run(command::SetSnap{.enabled = false}));
    ASSERT_TRUE(run(command::SetGridSize{.size = 32.F}));
    ASSERT_TRUE(run(command::SetOverlay{.overlay = Overlay::hitboxes, .visible = true}));
    ASSERT_TRUE(run(command::SetOverlay{.overlay = Overlay::names, .visible = false}));

    EXPECT_EQ(_state.view, (editor::ViewSettings{
                               .snap = false,
                               .grid_size = 32.F,
                               .show_hitboxes = true,
                               .show_names = false,
                           }));
    EXPECT_FALSE(_session.unsaved());
}

TEST_F(ApplyCommand, InvalidGridSizeIsRefused) {
    const float before = _state.view.grid_size;

    for (const float size : {0.F, -4.F, std::numeric_limits<float>::infinity(),
                             std::numeric_limits<float>::quiet_NaN()}) {
        EXPECT_FALSE(run(command::SetGridSize{.size = size})) << size;
    }
    EXPECT_EQ(_state.view.grid_size, before);
}

TEST_F(ApplyCommand, PlayPauseAndStopSwitchTheMode) {
    ASSERT_TRUE(run(command::Play{}));
    EXPECT_EQ(_state.mode, Mode::playing);

    ASSERT_TRUE(run(command::Pause{}));
    EXPECT_EQ(_state.mode, Mode::paused);

    ASSERT_TRUE(run(command::Play{}));
    EXPECT_EQ(_state.mode, Mode::playing);

    ASSERT_TRUE(run(command::Stop{}));
    EXPECT_EQ(_state.mode, Mode::editing);
}

TEST_F(ApplyCommand, ModeSwitchesThatMakeNoSenseAreRefused) {
    EXPECT_FALSE(run(command::Pause{}));
    EXPECT_FALSE(run(command::Stop{}));

    ASSERT_TRUE(run(command::Play{}));
    EXPECT_FALSE(run(command::Play{}));
    EXPECT_EQ(_state.mode, Mode::playing);
}

TEST_F(ApplyCommand, ToolAndTabChangeTheState) {
    ASSERT_TRUE(run(command::SetTool{.tool = editor::Tool::pan}));
    ASSERT_TRUE(run(command::ShowTab{.tab = editor::BottomTab::problems}));

    EXPECT_EQ(_state.tool, editor::Tool::pan);
    EXPECT_EQ(_state.bottom_tab, editor::BottomTab::problems);
}

TEST_F(ApplyCommand, PrefabFilterChangesTheState) {
    ASSERT_TRUE(run(command::SetPrefabFilter{.text = "ro"}));
    EXPECT_EQ(_state.prefab_filter, "ro");
}

TEST_F(ApplyCommand, InvalidInputIsRefusedWithItsMessage) {
    const auto result =
        apply_command(_session, _state, {.play = _play, .open_file = &_opener, .runtime = _runtime},
                      command::ReportInvalidInput{.message = "bad"});
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.message, "bad");
}

TEST_F(ApplyCommand, OpenScriptOpensTheFile) {
    ASSERT_TRUE(run(command::OpenScript{.file = "/game/scripts/rock.lua"}));
    EXPECT_EQ(_openings->opened, (std::vector<std::filesystem::path>{"/game/scripts/rock.lua"}));
}

TEST_F(ApplyCommand, OpenScriptFailureIsReported) {
    const auto result =
        apply_command(_session, _state, {.play = _play, .open_file = &_opener, .runtime = _runtime},
                      command::OpenScript{.file = "/game/scripts/locked.lua"});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("locked.lua"), std::string::npos);
}

TEST_F(ApplyCommand, OpenScriptWithoutAnOpenerIsRefused) {
    EXPECT_FALSE(apply_command(_session, _state,
                               {.play = _play, .open_file = nullptr, .runtime = _runtime},
                               command::OpenScript{.file = "/game/scripts/rock.lua"})
                     .ok);
}

namespace {

class ApplyExport : public ApplyCommand {
protected:
    std::filesystem::path _output = _game.path().string() + "-export";

    ApplyExport() {
        _runtime = _game.path() / ".runtime";
        _game.write(".runtime/engine-runtime", "runtime");
        _game.write(".runtime/plugins/graphics-raylib.so", "graphics");
    }

    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(_output, ignored);
    }

    [[nodiscard]] const editor::ExportDialog& dialog() const {
        if (!_state.export_dialog) {
            throw std::logic_error("the export dialog is not open");
        }
        return *_state.export_dialog;
    }

    void choose_output(const std::filesystem::path& output) {
        editor::ExportSettings settings = dialog().settings;
        settings.output = output;
        ASSERT_TRUE(run(command::SetExport{.settings = settings}));
    }
};

} // namespace

TEST_F(ApplyExport, OpenExportPlansTheDefaults) {
    ASSERT_TRUE(run(command::OpenExport{}));

    ASSERT_TRUE(_state.export_dialog.has_value());
    EXPECT_EQ(dialog().settings.name, "editor-test");
    EXPECT_TRUE(dialog().plan.problems.empty());
    EXPECT_FALSE(dialog().plan.steps.empty());
}

TEST_F(ApplyExport, SetExportPlansAgain) {
    ASSERT_TRUE(run(command::OpenExport{}));
    choose_output({});

    EXPECT_TRUE(dialog().settings.output.empty());
    EXPECT_FALSE(dialog().plan.problems.empty());
}

TEST_F(ApplyExport, DialogCommandsNeedTheDialog) {
    EXPECT_FALSE(run(command::SetExport{}));
    EXPECT_FALSE(run(command::RunExport{}));
}

TEST_F(ApplyExport, CloseExportClosesTheDialog) {
    ASSERT_TRUE(run(command::OpenExport{}));
    ASSERT_TRUE(run(command::CloseExport{}));

    EXPECT_FALSE(_state.export_dialog.has_value());
}

TEST_F(ApplyExport, ExportWithProblemsIsRefusedAndKeepsTheDialog) {
    ASSERT_TRUE(run(command::OpenExport{}));
    choose_output(_game.path() / "inside");

    const auto result =
        apply_command(_session, _state, {.play = _play, .open_file = &_opener, .runtime = _runtime},
                      command::RunExport{});

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("inside the game folder"), std::string::npos);
    EXPECT_TRUE(_state.export_dialog.has_value());
    EXPECT_FALSE(std::filesystem::exists(_game.path() / "inside"));
}

TEST_F(ApplyExport, ExportWritesTheGameAndClosesTheDialog) {
    ASSERT_TRUE(run(command::OpenExport{}));
    choose_output(_output);

    ASSERT_TRUE(run(command::RunExport{}));

    EXPECT_FALSE(_state.export_dialog.has_value());
    EXPECT_TRUE(std::filesystem::exists(_output / "editor-test"));
    EXPECT_TRUE(std::filesystem::exists(_output / "editor-test-server"));
    EXPECT_TRUE(std::filesystem::exists(_output / "scenes" / "level1.json"));
    EXPECT_TRUE(std::filesystem::exists(_output / "README.txt"));
}

TEST_F(ApplyCommand, OpenSettingsWithoutAFileIsRefused) {
    EXPECT_FALSE(run(command::OpenSettings{}));
    EXPECT_TRUE(_openings->opened.empty());
}

TEST_F(ApplyCommand, OpenSettingsCreatesTheFileAndOpensIt) {
    _state.settings_file = _game.path() / ".config" / "engine-editor" / "settings.json";

    ASSERT_TRUE(run(command::OpenSettings{}));

    EXPECT_EQ(_openings->opened, (std::vector<std::filesystem::path>{_state.settings_file}));
    EXPECT_EQ(_game.read(".config/engine-editor/settings.json"), "{}\n");
}

TEST_F(ApplyCommand, OpenSettingsKeepsAnExistingFile) {
    _game.write("settings.json", R"({"view.snap": false})");
    _state.settings_file = _game.path() / "settings.json";

    ASSERT_TRUE(run(command::OpenSettings{}));

    EXPECT_EQ(_game.read("settings.json"), R"({"view.snap": false})");
}

TEST_F(ApplyCommand, CreatePrefabWritesReloadsAndOpensTheScript) {
    _state.name_dialog = editor::NameDialog{.item = editor::NewItem::prefab, .name = "crate"};

    ASSERT_TRUE(run(command::CreatePrefab{.name = "crate"}));

    const auto file = _game.path() / "scripts" / "crate.lua";
    EXPECT_TRUE(std::filesystem::is_regular_file(file));
    EXPECT_EQ(_session.prefabs(), (std::vector<std::string>{"crate", "rock", "ship"}));
    EXPECT_EQ(_openings->opened, (std::vector<std::filesystem::path>{file}));
    EXPECT_FALSE(_state.name_dialog.has_value());
}

TEST_F(ApplyCommand, CreatePrefabRefusesADeclaredName) {
    _state.name_dialog = editor::NameDialog{.item = editor::NewItem::prefab, .name = "rock"};
    std::filesystem::rename(_game.path() / "scripts" / "rock.lua",
                            _game.path() / "scripts" / "stones.lua");

    EXPECT_FALSE(run(command::CreatePrefab{.name = "rock"}));

    EXPECT_FALSE(std::filesystem::exists(_game.path() / "scripts" / "rock.lua"));
    EXPECT_TRUE(_state.name_dialog.has_value());
    EXPECT_TRUE(_openings->opened.empty());
}

TEST_F(ApplyCommand, CreatePrefabRefusesABadName) {
    _state.name_dialog = editor::NameDialog{.item = editor::NewItem::prefab, .name = "a b"};

    EXPECT_FALSE(run(command::CreatePrefab{.name = "a b"}));

    EXPECT_TRUE(_state.name_dialog.has_value());
}

TEST_F(ApplyCommand, CreateSceneClosesTheNameDialog) {
    _state.name_dialog = editor::NameDialog{.item = editor::NewItem::scene, .name = "boss"};

    ASSERT_TRUE(run(command::CreateScene{.name = "boss"}));

    EXPECT_FALSE(_state.name_dialog.has_value());
    EXPECT_EQ(_session.open_scene_name(), "boss");
}

TEST_F(ApplyCommand, NameDialogOpensChangesAndCloses) {
    EXPECT_FALSE(run(command::SetNameDialogText{.name = "x"}));

    ASSERT_TRUE(run(command::OpenNameDialog{.item = editor::NewItem::scene}));
    ASSERT_TRUE(run(command::SetNameDialogText{.name = "boss"}));
    EXPECT_EQ(_state.name_dialog,
              (editor::NameDialog{.item = editor::NewItem::scene, .name = "boss"}));

    ASSERT_TRUE(run(command::CloseNameDialog{}));
    EXPECT_FALSE(_state.name_dialog.has_value());
}

TEST_F(ApplyCommand, RevealGameFolderOpensTheGameFolder) {
    ASSERT_TRUE(run(command::RevealGameFolder{}));

    EXPECT_EQ(_openings->opened, (std::vector<std::filesystem::path>{_session.game()}));
}
