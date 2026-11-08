#include <gtest/gtest.h>

#include "action/Action.hpp"
#include "action/ActionText.hpp"

using editor::describe_action;
using engine::editor::InstanceId;

namespace command = editor::command;
namespace edit = editor::edit;

TEST(ActionText, EditsShowTheirFields) {
    EXPECT_EQ(describe_action(edit::Move{.id = InstanceId{3}, .position = {9, 9.5F}}),
              "edit::Move id=3 position=(9, 9.5)");
    EXPECT_EQ(describe_action(edit::Place{.type = "rock", .position = {0, 16}}),
              "edit::Place type=rock position=(0, 16)");
    EXPECT_EQ(describe_action(edit::Duplicate{.id = InstanceId{4}, .offset = {16, 16}}),
              "edit::Duplicate id=4 offset=(16, 16)");
    EXPECT_EQ(describe_action(edit::ResetField{.id = InstanceId{2}, .key = "Scale.x"}),
              "edit::ResetField id=2 key=Scale.x");
}

TEST(ActionText, FieldValuesAreDescribed) {
    const std::string text = describe_action(edit::SetField{
        .id = InstanceId{1}, .key = "Health.hp", .value = engine::core::FieldValue{2.5F}});

    EXPECT_EQ(text.rfind("edit::SetField id=1 key=Health.hp value=", 0), 0U);
    EXPECT_NE(text.find("2.5"), std::string::npos);
}

TEST(ActionText, CommandsWithoutFieldsShowTheirName) {
    EXPECT_EQ(describe_action(command::Save{}), "command::Save");
    EXPECT_EQ(describe_action(command::RunExport{}), "command::RunExport");
}

TEST(ActionText, CommandsShowTheirFields) {
    EXPECT_EQ(describe_action(command::Select{.id = std::nullopt}), "command::Select id=none");
    EXPECT_EQ(describe_action(command::SetTool{.tool = editor::Tool::pan}),
              "command::SetTool tool=pan");
    EXPECT_EQ(
        describe_action(command::SetOverlay{.overlay = editor::Overlay::names, .visible = true}),
        "command::SetOverlay overlay=names visible=true");
    EXPECT_EQ(describe_action(command::ReloadScripts{}), "command::ReloadScripts");
    EXPECT_EQ(describe_action(command::ShowTab{.tab = editor::BottomTab::assets}),
              "command::ShowTab tab=assets");
    EXPECT_EQ(describe_action(command::CreatePrefab{.name = "crate"}),
              "command::CreatePrefab name=crate");
    EXPECT_EQ(describe_action(command::OpenNameDialog{.item = editor::NewItem::scene}),
              "command::OpenNameDialog item=scene");
    EXPECT_EQ(describe_action(command::SetNameDialogText{.name = "boss"}),
              "command::SetNameDialogText name=boss");
    EXPECT_EQ(describe_action(command::CloseNameDialog{}), "command::CloseNameDialog");
    EXPECT_EQ(describe_action(command::RevealGameFolder{}), "command::RevealGameFolder");
    EXPECT_EQ(describe_action(command::DuplicateScene{.from = "a", .to = "b"}),
              "command::DuplicateScene from=a to=b");
}

TEST(ActionText, EveryActionHasAName) {
    for (const editor::Action& action :
         {editor::Action{command::Undo{}}, editor::Action{command::Play{}},
          editor::Action{command::OpenExport{}}, editor::Action{edit::Remove{.id = InstanceId{4}}},
          editor::Action{command::ShowTab{.tab = editor::BottomTab::problems}}}) {
        EXPECT_FALSE(describe_action(action).empty());
    }
}
