#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "../../engine/script/scene/TempFolder.hpp"
#include "engine/editor/EditorSession.hpp"
#include "project/ProjectFiles.hpp"

using editor::create_prefab;
using editor::create_project;
using editor::prefab_file;
using engine::editor::EditorSession;

TEST(CreateProject, WritesAGameTheEditorOpens) {
    const TempFolder parent;
    const auto folder = parent.path() / "space-game";

    const auto result = create_project(folder);

    ASSERT_TRUE(result.ok) << result.message;
    auto session = EditorSession::open(folder);
    ASSERT_TRUE(session.has_value());
    EXPECT_TRUE(session->scripts_loaded());
    EXPECT_EQ(session->start_scene(), "main");
    EXPECT_EQ(session->scenes(), (std::vector<std::string>{"main"}));
    EXPECT_TRUE(session->open_scene("main").ok);
    EXPECT_TRUE(std::filesystem::is_directory(folder / "assets"));
}

TEST(CreateProject, MainScriptDefinesOnStart) {
    const TempFolder parent;

    ASSERT_TRUE(create_project(parent.path() / "game").ok);

    EXPECT_NE(parent.read("game/scripts/main.lua").find("function on_start()"), std::string::npos);
}

TEST(CreateProject, NamesTheGameAfterItsFolder) {
    const TempFolder parent;

    ASSERT_TRUE(create_project(parent.path() / "space-game").ok);

    EXPECT_NE(parent.read("space-game/game.json").find(R"("name": "space-game")"),
              std::string::npos);
}

TEST(CreateProject, FillsAnEmptyFolder) {
    const TempFolder folder;

    ASSERT_TRUE(create_project(folder.path()).ok);

    EXPECT_TRUE(std::filesystem::is_regular_file(folder.path() / "game.json"));
}

TEST(CreateProject, NamesTheGameAfterTheFolderOfADotPath) {
    const TempFolder folder;

    ASSERT_TRUE(create_project(folder.path() / ".").ok);

    EXPECT_NE(folder.read("game.json").find(folder.path().filename().string()), std::string::npos);
}

TEST(CreateProject, RefusesAFolderThatIsNotEmpty) {
    const TempFolder folder;
    folder.write("notes.txt", "keep me");

    const auto result = create_project(folder.path());

    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("not empty"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(folder.path() / "game.json"));
    EXPECT_EQ(folder.read("notes.txt"), "keep me");
}

TEST(CreateProject, RefusesAFile) {
    const TempFolder folder;
    folder.write("game", "");

    EXPECT_FALSE(create_project(folder.path() / "game").ok);
}

TEST(CreatePrefab, WritesAScriptDeclaringThePrefab) {
    const TempFolder game;
    ASSERT_TRUE(create_project(game.path()).ok);

    const auto result = create_prefab(game.path(), "crate");

    ASSERT_TRUE(result.ok) << result.message;
    EXPECT_EQ(prefab_file(game.path(), "crate"), game.path() / "scripts" / "crate.lua");
    auto session = EditorSession::open(game.path());
    ASSERT_TRUE(session.has_value());
    EXPECT_TRUE(session->scripts_loaded());
    EXPECT_EQ(session->prefabs(), (std::vector<std::string>{"crate"}));
}

TEST(CreatePrefab, RefusesAnExistingFile) {
    const TempFolder game;
    game.write("scripts/crate.lua", "entity \"crate\"\n");

    EXPECT_FALSE(create_prefab(game.path(), "crate").ok);
    EXPECT_EQ(game.read("scripts/crate.lua"), "entity \"crate\"\n");
}

TEST(CreatePrefab, RefusesNamesThatAreNotFileNames) {
    const TempFolder game;

    for (const std::string& name :
         std::vector<std::string>{"", "a b", "../escape", "dots.lua", std::string(64, 'a')}) {
        EXPECT_FALSE(create_prefab(game.path(), name).ok) << name;
    }
    EXPECT_FALSE(std::filesystem::exists(game.path() / "scripts"));
}
