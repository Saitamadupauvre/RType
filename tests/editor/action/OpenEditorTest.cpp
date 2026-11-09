#include <gtest/gtest.h>

#include "../GameFixture.hpp"
#include "action/OpenEditor.hpp"

using editor::open_editor;

namespace {

using OpenEditor = GameFixture;

} // namespace

TEST_F(OpenEditor, OpensTheStartScene) {
    const auto editor = open_editor(_game.path());

    ASSERT_TRUE(editor.has_value());
    EXPECT_EQ(editor->session().open_scene_name(), "level1");
}

TEST_F(OpenEditor, ListsPrefabsAndScenes) {
    const auto editor = open_editor(_game.path());

    ASSERT_TRUE(editor.has_value());
    EXPECT_FALSE(editor->state().prefabs.empty());
    EXPECT_EQ(editor->state().scenes,
              (std::vector<std::string>{"broken", "faulty", "level1", "level2"}));
}

TEST_F(OpenEditor, MissingStartSceneLeavesNothingOpen) {
    _game.write("game.json", R"({"name": "editor-test"})");

    const auto editor = open_editor(_game.path());

    ASSERT_TRUE(editor.has_value());
    EXPECT_FALSE(editor->session().open_scene_name().has_value());
}

TEST_F(OpenEditor, NotAFolderIsRefused) {
    EXPECT_FALSE(open_editor(_game.path() / "game.json").has_value());
    EXPECT_FALSE(open_editor(_game.path() / "nowhere").has_value());
}
