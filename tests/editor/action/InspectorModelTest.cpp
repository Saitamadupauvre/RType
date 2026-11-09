#include <gtest/gtest.h>

#include <filesystem>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../GameFixture.hpp"
#include "action/Action.hpp"
#include "action/Editor.hpp"
#include "action/InspectorModel.hpp"

using editor::InspectorField;
using engine::core::FieldKind;
using engine::core::FieldValue;

namespace edit = editor::edit;
namespace command = editor::command;

namespace {

class InspectorModel : public GameFixture {
protected:
    engine::editor::EditorSession _session{open_level()};

    std::optional<editor::InspectorModel> inspect(std::string_view type) {
        _session.select(id_of(_session, type));
        return editor::inspect_selection(_session);
    }

    static std::vector<std::string> keys(const std::vector<InspectorField>& fields) {
        std::vector<std::string> result;
        std::ranges::transform(fields, std::back_inserter(result), &InspectorField::key);
        return result;
    }

    static const InspectorField* find(const editor::InspectorModel& model, std::string_view key) {
        for (const auto* fields : {&model.fields, &model.script_fields}) {
            const auto found = std::ranges::find(*fields, key, &InspectorField::key);
            if (found != fields->end()) {
                return &*found;
            }
        }
        return nullptr;
    }
};

class InspectorState : public GameFixture {
protected:
    editor::Editor _editor{open_level()};

    [[nodiscard]] const InspectorField* health() const {
        const auto& model = _editor.state().inspector;
        if (!model) {
            return nullptr;
        }
        const auto found =
            std::ranges::find(model->script_fields, "Script.health", &InspectorField::key);
        return found == model->script_fields.end() ? nullptr : &*found;
    }
};

} // namespace

TEST_F(InspectorModel, NothingWithoutASelection) {
    EXPECT_FALSE(editor::inspect_selection(_session).has_value());
}

TEST_F(InspectorModel, ListsRegistryFieldsThenScriptFieldsInOrder) {
    const auto model = inspect("rock");
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(keys(model->fields), std::vector<std::string>{"Rotation.degrees"});
    EXPECT_EQ(keys(model->script_fields),
              (std::vector<std::string>{"Script.armored", "Script.drift", "Script.label"}));
}

TEST_F(InspectorModel, LeavesThePositionOut) {
    const auto model = inspect("rock");
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(find(*model, "Position.pos"), nullptr);
}

TEST_F(InspectorModel, GivesEachFieldItsKind) {
    const auto model = inspect("rock");
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(find(*model, "Rotation.degrees")->kind, FieldKind::Float);
    EXPECT_EQ(find(*model, "Script.armored")->kind, FieldKind::Bool);
    EXPECT_EQ(find(*model, "Script.drift")->kind, FieldKind::Vec2);
    EXPECT_EQ(find(*model, "Script.label")->kind, FieldKind::String);
}

TEST_F(InspectorModel, OverrideComesFromTheInstance) {
    const auto model = inspect("ship");
    ASSERT_TRUE(model.has_value());
    const InspectorField* health = find(*model, "Script.health");
    ASSERT_NE(health, nullptr);
    EXPECT_TRUE(health->overridden);
    EXPECT_EQ(health->value, FieldValue{std::int64_t{7}});
}

TEST_F(InspectorModel, OtherFieldsShowThePrefabValue) {
    const auto model = inspect("ship");
    ASSERT_TRUE(model.has_value());
    const InspectorField* color = find(*model, "Sprite.color");
    ASSERT_NE(color, nullptr);
    EXPECT_FALSE(color->overridden);
    const FieldValue expected{engine::core::Rgba{.r = 10, .g = 20, .b = 30, .a = 255}};
    EXPECT_EQ(color->value, expected);
}

TEST_F(InspectorState, FollowsTheSelection) {
    EXPECT_FALSE(_editor.state().inspector.has_value());
    _editor.handle(command::Select{.id = id_of(_editor.session(), "rock")});
    ASSERT_TRUE(_editor.state().inspector.has_value());
    EXPECT_EQ(_editor.state().inspector->id, id_of(_editor.session(), "rock"));
}

TEST_F(InspectorState, ShowsAFieldSetByAnEdit) {
    const auto hero = id_of(_editor.session(), "ship");
    _editor.handle(command::Select{.id = hero});
    _editor.handle(edit::SetField{.id = hero, .key = "Script.health", .value = std::int64_t{9}});
    ASSERT_NE(health(), nullptr);
    EXPECT_EQ(health()->value, FieldValue{std::int64_t{9}});
}

TEST_F(InspectorState, ShowsThePrefabValueAfterAReset) {
    const auto hero = id_of(_editor.session(), "ship");
    _editor.handle(command::Select{.id = hero});
    _editor.handle(edit::ResetField{.id = hero, .key = "Script.health"});
    ASSERT_NE(health(), nullptr);
    EXPECT_FALSE(health()->overridden);
    EXPECT_EQ(health()->value, FieldValue{std::int64_t{3}});
}

TEST_F(InspectorState, FollowsUndo) {
    const auto hero = id_of(_editor.session(), "ship");
    _editor.handle(command::Select{.id = hero});
    _editor.handle(edit::ResetField{.id = hero, .key = "Script.health"});
    _editor.handle(command::Undo{});
    ASSERT_NE(health(), nullptr);
    EXPECT_TRUE(health()->overridden);
}

TEST_F(InspectorState, ClearsWhenTheSelectedInstanceIsRemoved) {
    const auto rock = id_of(_editor.session(), "rock");
    _editor.handle(command::Select{.id = rock});
    _editor.handle(edit::Remove{.id = rock});
    EXPECT_FALSE(_editor.state().inspector.has_value());
}

TEST_F(InspectorModel, KnowsTheScriptOfThePrefab) {
    const auto model = inspect("rock");
    ASSERT_TRUE(model.has_value());
    EXPECT_EQ(model->script.filename(), "rock.lua");
    EXPECT_TRUE(std::filesystem::is_regular_file(model->script));
}
