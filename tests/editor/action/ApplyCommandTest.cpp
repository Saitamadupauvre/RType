#include <gtest/gtest.h>

#include <limits>
#include <optional>

#include "../GameFixture.hpp"
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
    InstanceId _rock = id_of(_session, "rock");

    void SetUp() override {
        _play.attach(std::make_unique<FakeLauncher>(std::make_shared<FakeGame>()));
    }

    bool run(const editor::Command& command) {
        return apply_command(_session, _state, _play, command).ok;
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
        apply_command(_session, _state, _play, command::ReportInvalidInput{.message = "bad"});
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.message, "bad");
}
