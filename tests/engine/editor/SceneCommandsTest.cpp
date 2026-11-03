#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "EditorFixture.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

using engine::core::Vec2;
using engine::editor::CommandResult;
using engine::editor::EditorSession;

namespace {

using SceneCommandsTest = EditorFixture;

} // namespace

TEST_F(SceneCommandsTest, CreateWritesAnEmptySceneAndOpensIt) {
    ASSERT_TRUE(session().create_scene("level3").ok);

    EXPECT_TRUE(scene_on_disk("level3"));
    EXPECT_EQ(session().open_scene_name(), "level3");
    EXPECT_TRUE(session().document().instances.empty());
    EXPECT_FALSE(session().unsaved());
    EXPECT_NE(_game.read("scenes/level3.json").find("\"version\": 1"), std::string::npos);
}

TEST_F(SceneCommandsTest, InvalidOrTakenNamesAreRefused) {
    for (const std::string name : {"", "../escape", "a/b", "level1"}) {
        EXPECT_FALSE(session().create_scene(name).ok) << name;
    }
    EXPECT_FALSE(session().duplicate_scene("level1", "level2").ok);
    EXPECT_FALSE(session().duplicate_scene("level1", "../x").ok);
    EXPECT_FALSE(session().rename_scene("level1", "level2").ok);
    EXPECT_FALSE(session().rename_scene("nope", "other").ok);
    EXPECT_FALSE(session().open_scene("nope").ok);
    EXPECT_FALSE(session().open_scene("../game").ok);
    EXPECT_FALSE(session().delete_scene("nope").ok);
    EXPECT_FALSE(session().set_start_scene("nope").ok);
    EXPECT_FALSE(scene_on_disk("escape"));
}

TEST_F(SceneCommandsTest, OpenReportsSkippedInstances) {
    const CommandResult result = session().open_scene("level1");

    ASSERT_TRUE(result.ok);
    EXPECT_NE(result.message.find("1 instance"), std::string::npos);
    EXPECT_TRUE(session().open_scene("level2").message.empty());
}

TEST_F(SceneCommandsTest, RejectedFileIsNotOpened) {
    const CommandResult result = session().open_scene("broken");

    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(session().open_scene_name().has_value());
}

TEST_F(SceneCommandsTest, SaveWritesTheDocument) {
    ASSERT_TRUE(session().open_scene("level2").ok);
    ASSERT_TRUE(session().place("rock", Vec2{3, 4}).ok);
    ASSERT_TRUE(session().unsaved());

    ASSERT_TRUE(session().save().ok);
    EXPECT_FALSE(session().unsaved());
    EXPECT_TRUE(session().can_undo());

    auto reopened = EditorSession::open(_game.path());
    ASSERT_TRUE(reopened.has_value());
    ASSERT_TRUE(reopened->open_scene("level2").ok);
    ASSERT_EQ(reopened->document().instances.size(), 1U);
    EXPECT_EQ(reopened->document().instances[0].instance.position, (Vec2{3, 4}));
}

TEST_F(SceneCommandsTest, SaveKeepsUnknownInstances) {
    ASSERT_TRUE(session().open_scene("level1").ok);
    ASSERT_TRUE(session().save().ok);

    EXPECT_NE(_game.read("scenes/level1.json").find("ghost"), std::string::npos);
}

TEST_F(SceneCommandsTest, UnsavedChangesBlockSwitchingScenes) {
    ASSERT_TRUE(session().open_scene("level2").ok);
    ASSERT_TRUE(session().place("rock", Vec2{0, 0}).ok);

    EXPECT_FALSE(session().open_scene("level1").ok);
    EXPECT_FALSE(session().create_scene("level3").ok);
    EXPECT_FALSE(session().delete_scene("level2").ok);
    EXPECT_EQ(session().open_scene_name(), "level2");
    EXPECT_TRUE(scene_on_disk("level2"));
    EXPECT_FALSE(scene_on_disk("level3"));
}

TEST_F(SceneCommandsTest, DiscardGoesBackToTheSavedScene) {
    ASSERT_TRUE(session().open_scene("level2").ok);
    ASSERT_TRUE(session().place("rock", Vec2{0, 0}).ok);

    ASSERT_TRUE(session().discard_changes().ok);
    EXPECT_TRUE(session().document().instances.empty());
    EXPECT_FALSE(session().unsaved());
    EXPECT_FALSE(session().can_undo());
    EXPECT_FALSE(session().selection().has_value());
    EXPECT_TRUE(session().open_scene("level1").ok);
}

TEST_F(SceneCommandsTest, DuplicateCopiesTheSavedFile) {
    ASSERT_TRUE(session().open_scene("level1").ok);
    ASSERT_TRUE(session().place("rock", Vec2{0, 0}).ok);

    ASSERT_TRUE(session().duplicate_scene("level1", "copy").ok);
    EXPECT_EQ(_game.read("scenes/copy.json"), _game.read("scenes/level1.json"));
    EXPECT_EQ(session().open_scene_name(), "level1");
}

TEST_F(SceneCommandsTest, RenamingTheStartSceneUpdatesGameJson) {
    ASSERT_TRUE(session().open_scene("level1").ok);

    ASSERT_TRUE(session().rename_scene("level1", "intro").ok);
    EXPECT_FALSE(scene_on_disk("level1"));
    EXPECT_TRUE(scene_on_disk("intro"));
    EXPECT_EQ(session().start_scene(), "intro");
    EXPECT_EQ(session().open_scene_name(), "intro");
    const std::string project = _game.read("game.json");
    EXPECT_NE(project.find("\"start_scene\": \"intro\""), std::string::npos);
    EXPECT_NE(project.find("\"window\""), std::string::npos);
}

TEST_F(SceneCommandsTest, RenamingAnotherSceneLeavesGameJson) {
    const std::string before = _game.read("game.json");

    ASSERT_TRUE(session().rename_scene("level2", "outro").ok);
    EXPECT_EQ(_game.read("game.json"), before);
    EXPECT_EQ(session().start_scene(), "level1");
}

TEST_F(SceneCommandsTest, DeleteRemovesAndClosesTheScene) {
    ASSERT_TRUE(session().open_scene("level2").ok);

    ASSERT_TRUE(session().delete_scene("level2").ok);
    EXPECT_FALSE(scene_on_disk("level2"));
    EXPECT_FALSE(session().open_scene_name().has_value());
}

TEST_F(SceneCommandsTest, StartSceneCannotBeDeleted) {
    const CommandResult result = session().delete_scene("level1");

    EXPECT_FALSE(result.ok);
    EXPECT_TRUE(scene_on_disk("level1"));
}

TEST_F(SceneCommandsTest, SetStartSceneWritesGameJson) {
    ASSERT_TRUE(session().set_start_scene("level2").ok);

    EXPECT_EQ(session().start_scene(), "level2");
    auto reopened = EditorSession::open(_game.path());
    ASSERT_TRUE(reopened.has_value());
    EXPECT_EQ(reopened->start_scene(), "level2");
    EXPECT_TRUE(session().delete_scene("level1").ok);
}
