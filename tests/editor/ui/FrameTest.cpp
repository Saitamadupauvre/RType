#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <string>

#include "FrameFixture.hpp"
#include "action/Action.hpp"
#include "engine/editor/SceneDocument.hpp"

using editor::BottomTab;
using editor::Overlay;
using editor::Tool;
using engine::editor::InstanceId;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

using FrameTest = FrameFixture;

std::ptrdiff_t count(const std::vector<std::string>& texts, std::string_view text) {
    return std::ranges::count(texts, text);
}

bool contains(const std::vector<std::string>& texts, std::string_view part) {
    return std::ranges::any_of(
        texts, [&](const std::string& text) { return text.find(part) != std::string::npos; });
}

} // namespace

TEST_F(FrameTest, NoInputEmitsNothing) {
    EXPECT_TRUE(frame().actions.empty());
    EXPECT_TRUE(frame().actions.empty());
}

TEST_F(FrameTest, HoveringWithoutPressingEmitsNothing) {
    frame();
    const Clay_BoundingBox play = box_of(CLAY_ID("PlayButton"));

    EXPECT_TRUE(frame({.pointer = {.x = play.x + 1.F, .y = play.y + 1.F}}).actions.empty());
}

TEST_F(FrameTest, PressingOnEmptySpaceEmitsNothing) {
    EXPECT_TRUE(click(CLAY_ID("Viewport")).empty());
}

TEST_F(FrameTest, PlayButtonEmitsPlay) {
    const auto actions = click(CLAY_ID("PlayButton"));
    EXPECT_NE(only<command::Play>(actions), nullptr);
}

TEST_F(FrameTest, PlayButtonIsDisabledWhilePlaying) {
    _editor.handle(command::Play{});

    EXPECT_TRUE(click(CLAY_ID("PlayButton")).empty());
}

TEST_F(FrameTest, StopButtonIsDisabledInEditMode) {
    EXPECT_TRUE(click(CLAY_ID("StopButton")).empty());

    _editor.handle(command::Play{});
    const auto actions = click(CLAY_ID("StopButton"));
    EXPECT_NE(only<command::Stop>(actions), nullptr);
}

TEST_F(FrameTest, ToolButtonsEmitSetTool) {
    const auto actions = click(CLAY_ID("ToolMove"));
    const auto* set = only<command::SetTool>(actions);
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->tool, Tool::move);
}

TEST_F(FrameTest, TogglesEmitTheOppositeValue) {
    const auto snap = click(CLAY_ID("SnapToggle"));
    ASSERT_NE(only<command::SetSnap>(snap), nullptr);
    EXPECT_FALSE(only<command::SetSnap>(snap)->enabled);

    const auto hitboxes = click(CLAY_ID("HitboxesToggle"));
    ASSERT_NE(only<command::SetOverlay>(hitboxes), nullptr);
    EXPECT_EQ(only<command::SetOverlay>(hitboxes)->overlay, Overlay::hitboxes);
    EXPECT_TRUE(only<command::SetOverlay>(hitboxes)->visible);
}

TEST_F(FrameTest, UndoIsDisabledWithoutHistory) {
    EXPECT_TRUE(click(CLAY_ID("UndoButton")).empty());

    _editor.handle(edit::Move{.id = id_of(_editor.session(), "rock"), .position = {9, 9}});
    const auto actions = click(CLAY_ID("UndoButton"));
    EXPECT_NE(only<command::Undo>(actions), nullptr);
}

TEST_F(FrameTest, UndoIsDisabledWhilePlaying) {
    _editor.handle(edit::Move{.id = id_of(_editor.session(), "rock"), .position = {9, 9}});
    _editor.handle(command::Play{});

    EXPECT_TRUE(click(CLAY_ID("UndoButton")).empty());
}

TEST_F(FrameTest, SceneRowOpensThatScene) {
    const auto actions = click(CLAY_IDI("SceneRow", 3));
    const auto* open = only<command::OpenScene>(actions);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->name, "level2");
}

TEST_F(FrameTest, OpenSceneRowEmitsNothing) { EXPECT_TRUE(click(CLAY_IDI("SceneRow", 2)).empty()); }

TEST_F(FrameTest, InstanceRowSelectsThatInstance) {
    const InstanceId hero = id_of(_editor.session(), "ship");

    const auto actions = click(CLAY_IDI("InstanceRow", 0));
    const auto* select = only<command::Select>(actions);
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->id, hero);
}

TEST_F(FrameTest, InactiveTabEmitsShowTab) {
    EXPECT_TRUE(click(CLAY_ID("ConsoleTab")).empty());

    const auto actions = click(CLAY_ID("ProblemsTab"));
    const auto* show = only<command::ShowTab>(actions);
    ASSERT_NE(show, nullptr);
    EXPECT_EQ(show->tab, BottomTab::problems);
}

TEST_F(FrameTest, ShowsTheOpenSceneAndItsInstances) {
    const auto texts = frame().texts;

    EXPECT_TRUE(contains(texts, "level1"));
    EXPECT_TRUE(contains(texts, "hero"));
    EXPECT_TRUE(contains(texts, "rock"));
    EXPECT_TRUE(contains(texts, "edit mode"));
    EXPECT_TRUE(contains(texts, "Nothing selected"));
}

TEST_F(FrameTest, MarksUnsavedChanges) {
    _editor.handle(edit::Move{.id = id_of(_editor.session(), "rock"), .position = {9, 9}});

    EXPECT_TRUE(shows("level1 •"));
}

TEST_F(FrameTest, InspectorShowsTheSelection) {
    _editor.handle(command::Select{.id = id_of(_editor.session(), "ship")});
    const auto texts = frame().texts;

    EXPECT_TRUE(contains(texts, "INSPECTOR"));
    EXPECT_TRUE(contains(texts, "hero"));
    EXPECT_TRUE(contains(texts, "Script fields"));
    EXPECT_TRUE(contains(texts, "health"));
}

TEST_F(FrameTest, ConsoleShowsRefusedCommands) {
    _editor.handle(command::Undo{});

    EXPECT_TRUE(contains(frame().texts, _editor.state().console.front().text));
}

TEST_F(FrameTest, ClickingAConsoleLineThatNamesAScriptOpensIt) {
    _editor.handle(editor::command::ReportInvalidInput{.message = "rock.lua:2: boom"});
    _editor.handle(editor::command::ReportInvalidInput{.message = "no script here"});

    const auto actions = click(CLAY_IDI("ConsoleLine", 0));
    const auto* open = only<editor::command::OpenScript>(actions);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->file, _game.path() / "scripts" / "rock.lua");
    EXPECT_TRUE(click(CLAY_IDI("ConsoleLine", 1)).empty());
}

TEST_F(FrameTest, ProblemsTabListsProblems) {
    _editor.handle(command::OpenScene{.name = "faulty"});
    _editor.handle(command::ShowTab{.tab = BottomTab::problems});
    const auto texts = frame().texts;

    EXPECT_TRUE(contains(texts, "Problems 1"));
    EXPECT_TRUE(contains(texts, "PhysicsBody.type"));
}

TEST_F(FrameTest, ProblemsTabListsScriptWarnings) {
    _game.write("scripts/crate.lua", "entity \"crate\" {}\nfunction update(self, dt) end\n");
    _editor.handle(command::ReloadScripts{});
    _editor.handle(command::ShowTab{.tab = BottomTab::problems});
    const auto texts = frame().texts;

    EXPECT_TRUE(contains(texts, "Problems 1"));
    EXPECT_TRUE(std::ranges::any_of(texts, [](const std::string& text) {
        return text.starts_with("crate.lua: 'update' is defined outside");
    }));
}

TEST_F(FrameTest, TypingInThePrefabFilterEmitsSetPrefabFilter) {
    click(CLAY_ID("PrefabFilter"));
    frame();
    const auto actions = frame({.typed = "ro"}).actions;

    const auto* filter = only<command::SetPrefabFilter>(actions);
    ASSERT_NE(filter, nullptr);
    EXPECT_EQ(filter->text, "ro");
}

TEST_F(FrameTest, PrefabFilterHidesPrefabsThatDoNotMatch) {
    const std::ptrdiff_t ships = count(frame().texts, "ship");
    _editor.handle(command::SetPrefabFilter{.text = "RO"});

    const auto texts = frame().texts;
    EXPECT_EQ(count(texts, "ship"), ships - 1);
    EXPECT_FALSE(contains(texts, "No prefab matches"));
}

TEST_F(FrameTest, EmptyPrefabFilterShowsEveryPrefab) {
    const auto texts = frame().texts;
    EXPECT_EQ(count(texts, "rock"), 2);
    EXPECT_FALSE(contains(texts, "No prefab matches"));
}

TEST_F(FrameTest, PrefabFilterWithoutMatchShowsAHint) {
    _editor.handle(command::SetPrefabFilter{.text = "zzz"});
    EXPECT_TRUE(shows("No prefab matches"));
}

TEST_F(FrameTest, PlayingShowsTheBanner) {
    EXPECT_FALSE(shows("PLAYING"));

    _editor.handle(command::Play{});
    EXPECT_TRUE(shows("PLAYING"));

    _editor.handle(command::Pause{});
    EXPECT_TRUE(shows("PAUSED"));

    _editor.handle(command::Stop{});
    EXPECT_FALSE(shows("PAUSED"));
}

TEST_F(FrameTest, StatusBarShowsTheSelectionSnapAndUnsavedChanges) {
    _editor.handle(command::Select{.id = id_of(_editor.session(), "ship")});
    _editor.handle(edit::Move{.id = id_of(_editor.session(), "ship"), .position = {64, 64}});
    const auto texts = frame().texts;

    EXPECT_TRUE(contains(texts, "selected: hero (ship)"));
    EXPECT_TRUE(contains(texts, "snap 16"));
    EXPECT_TRUE(contains(texts, "unsaved changes · Ctrl+S"));

    _editor.handle(command::SetSnap{.enabled = false});
    EXPECT_TRUE(contains(frame().texts, "snap off"));
}

TEST_F(FrameTest, PanelHeadersCountTheirItems) {
    const auto texts = frame().texts;

    EXPECT_EQ(count(texts, "PREFABS"), 1);
    EXPECT_TRUE(contains(texts, "4"));
    EXPECT_TRUE(contains(texts, "2"));
}

TEST_F(FrameTest, ConsoleLinesShowTheirTime) {
    _editor.attach_clock([] { return std::string("12:04:31"); });
    _editor.handle(command::Undo{});

    const auto texts = frame().texts;
    EXPECT_TRUE(contains(texts, "12:04:31"));
}
