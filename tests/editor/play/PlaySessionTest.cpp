#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "../GameFixture.hpp"
#include "FakeLauncher.hpp"
#include "action/ApplyEdit.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/script/GameFolder.hpp"
#include "play/PlaySession.hpp"

using editor::ConsoleLine;
using editor::Mode;
using editor::PlaySession;
using editor::Severity;
using engine::core::Vec2;

namespace {

class Play : public GameFixture {
protected:
    engine::editor::EditorSession _session = open_level();
    std::shared_ptr<FakeGame> _fake = std::make_shared<FakeGame>();
    std::filesystem::path _scratch = _game.path() / ".scratch";
    PlaySession _play{_scratch};
    Mode _mode = Mode::editing;

    void SetUp() override { _play.attach(std::make_unique<FakeLauncher>(_fake)); }

    [[nodiscard]] std::filesystem::path played_file() const {
        return _fake->launches.empty() ? std::filesystem::path{}
                                       : _fake->launches.back().scene_file;
    }
};

} // namespace

TEST_F(Play, StartsTheGameFolderFromAFileOutsideIt) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);

    EXPECT_EQ(_mode, Mode::playing);
    ASSERT_EQ(_fake->launches.size(), 1U);
    EXPECT_EQ(_fake->launches[0].game, _session.game());
    EXPECT_EQ(played_file().parent_path(), _scratch);
    EXPECT_TRUE(std::filesystem::is_regular_file(played_file()));
}

TEST_F(Play, PlayedFileHoldsUnsavedChanges) {
    const auto rock = id_of(_session, "rock");
    ASSERT_TRUE(
        editor::apply_edit(_session, editor::edit::Move{.id = rock, .position = {40, 50}}).ok);

    ASSERT_TRUE(_play.play(_session, _mode).ok);

    const auto load = _session.runtime().read_scene(played_file());
    ASSERT_TRUE(load.error.empty()) << load.error;
    const auto& instances = load.scene.instances;
    EXPECT_TRUE(std::ranges::any_of(instances, [](const auto& instance) {
        return instance.type == "rock" && instance.position == Vec2(40, 50);
    }));
}

TEST_F(Play, PlayThenStopLeavesTheDocumentAndTheSceneFilesAlone) {
    const auto document = _session.document();
    const auto saved = engine::script::scene_file(_session.game(), "level1");
    const auto written = std::filesystem::last_write_time(saved);

    ASSERT_TRUE(_play.play(_session, _mode).ok);
    ASSERT_TRUE(_play.stop(_mode).ok);

    EXPECT_EQ(_session.document(), document);
    EXPECT_FALSE(_session.unsaved());
    EXPECT_EQ(std::filesystem::last_write_time(saved), written);
}

TEST_F(Play, StopEndsTheGameAndRemovesThePlayedFile) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);
    const auto file = played_file();

    ASSERT_TRUE(_play.stop(_mode).ok);

    EXPECT_EQ(_mode, Mode::editing);
    EXPECT_TRUE(_fake->stopped);
    EXPECT_FALSE(std::filesystem::exists(file));
}

TEST_F(Play, PauseAndPlayAgainResumeTheSameGame) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);

    ASSERT_TRUE(_play.pause(_mode).ok);
    EXPECT_EQ(_mode, Mode::paused);
    EXPECT_TRUE(_fake->paused);

    ASSERT_TRUE(_play.play(_session, _mode).ok);
    EXPECT_EQ(_mode, Mode::playing);
    EXPECT_FALSE(_fake->paused);
    EXPECT_EQ(_fake->launches.size(), 1U);
}

TEST_F(Play, ModeSwitchesThatMakeNoSenseAreRefused) {
    EXPECT_FALSE(_play.pause(_mode).ok);
    EXPECT_FALSE(_play.stop(_mode).ok);

    ASSERT_TRUE(_play.play(_session, _mode).ok);
    EXPECT_FALSE(_play.play(_session, _mode).ok);
    EXPECT_EQ(_fake->launches.size(), 1U);
}

TEST_F(Play, OutputGoesToTheConsole) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);
    _fake->output = {"server: started", "lua: hello"};

    const std::vector<ConsoleLine> lines = _play.update(_mode);

    EXPECT_EQ(lines,
              (std::vector<ConsoleLine>{{.severity = Severity::info, .text = "server: started"},
                                        {.severity = Severity::info, .text = "lua: hello"}}));
    EXPECT_EQ(_mode, Mode::playing);
}

TEST_F(Play, GameEndingOnItsOwnReturnsToEditing) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);
    _fake->exit = 84;

    const std::vector<ConsoleLine> lines = _play.update(_mode);

    EXPECT_EQ(_mode, Mode::editing);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.back(),
              (ConsoleLine{.severity = Severity::error, .text = "The game exited with code 84."}));
}

TEST_F(Play, CleanExitIsNotAnError) {
    ASSERT_TRUE(_play.play(_session, _mode).ok);
    _fake->exit = 0;

    EXPECT_EQ(_play.update(_mode).back().severity, Severity::info);
}

TEST_F(Play, LaunchFailureIsRefused) {
    _fake->refusal = "no game runtime at /nowhere";

    const auto result = _play.play(_session, _mode);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.message, "no game runtime at /nowhere");
    EXPECT_EQ(_mode, Mode::editing);
}

TEST_F(Play, WithoutALauncherPlayIsRefused) {
    PlaySession unattached{_scratch};

    EXPECT_FALSE(unattached.play(_session, _mode).ok);
    EXPECT_EQ(_mode, Mode::editing);
}

TEST_F(Play, WithoutAnOpenSceneThereIsNothingToPlay) {
    auto session = engine::editor::EditorSession::open(_game.path());
    ASSERT_TRUE(session.has_value());

    EXPECT_FALSE(_play.play(*session, _mode).ok);
    EXPECT_TRUE(_fake->launches.empty());
}
