#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "../FrameFixture.hpp"
#include "action/Action.hpp"
#include "engine/editor/SceneDocument.hpp"

using editor::Key;
using engine::core::FieldValue;
using engine::editor::InstanceId;

namespace command = editor::command;
namespace edit = editor::edit;

namespace {

class Inspector : public FrameFixture {
protected:
    void select(std::string_view type) {
        _editor.handle(command::Select{.id = id_of(_editor.session(), type)});
    }

    std::vector<editor::Action> press_enter_on(Clay_ElementId id) {
        click(id);
        frame();
        return frame({.keys_pressed = {Key::enter}}).actions;
    }

    static float right_of(Clay_ElementId id) {
        const Clay_BoundingBox box = box_of(id);
        return box.x + box.width;
    }

    std::vector<editor::Action> replace_text(Clay_ElementId id, std::string_view text) {
        click(id);
        frame();
        frame({.keys_pressed = {Key::a}, .primary_modifier = true});
        frame({.typed = std::string(text)});
        return frame({.keys_pressed = {Key::enter}}).actions;
    }

    [[nodiscard]] std::uint32_t index_of(std::string_view key) const {
        const auto& model = *_editor.state().inspector;
        std::uint32_t index = 0;
        for (const auto* fields : {&model.fields, &model.script_fields}) {
            for (const auto& field : *fields) {
                if (field.key == key) {
                    return index;
                }
                ++index;
            }
        }
        ADD_FAILURE() << "no field " << key;
        return index;
    }

    template <typename Wanted>
    static const Wanted* only_edit(const std::vector<editor::Action>& actions) {
        if (actions.size() != 1) {
            return nullptr;
        }
        const auto* change = std::get_if<editor::Edit>(&actions.front());
        return change ? std::get_if<Wanted>(change) : nullptr;
    }
};

} // namespace

TEST_F(Inspector, ShowsTheInstanceAndItsFields) {
    select("ship");

    EXPECT_TRUE(shows("INSPECTOR · hero"));
    EXPECT_TRUE(shows("hero"));
    EXPECT_TRUE(shows("ship"));
    EXPECT_TRUE(shows("COMPONENTS"));
    EXPECT_TRUE(shows("Sprite"));
    EXPECT_TRUE(shows("color"));
    EXPECT_FALSE(shows("Sprite.color"));
    EXPECT_TRUE(shows("#0A141EFF"));
    EXPECT_TRUE(shows("SCRIPT"));
    EXPECT_TRUE(shows("health"));
    EXPECT_TRUE(shows("7"));
}

TEST_F(Inspector, ShowsNothingSelectedWithoutASelection) {
    EXPECT_TRUE(shows("Nothing selected"));
    EXPECT_FALSE(shows("COMPONENTS"));
}

TEST_F(Inspector, RenamingEmitsRename) {
    select("ship");
    const auto actions = replace_text(CLAY_ID("InspectorName"), "boss");

    const auto* rename = only_edit<edit::Rename>(actions);
    ASSERT_NE(rename, nullptr);
    EXPECT_EQ(rename->id, id_of(_editor.session(), "ship"));
    EXPECT_EQ(rename->name, "boss");
}

TEST_F(Inspector, KeepingTheSameNameEmitsNothing) {
    select("ship");
    EXPECT_TRUE(replace_text(CLAY_ID("InspectorName"), "hero").empty());
}

TEST_F(Inspector, EditingAPositionCoordinateMovesAndKeepsTheOther) {
    select("ship");
    const auto actions = replace_text(CLAY_ID("InspectorPositionX"), "9");

    const auto* move = only_edit<edit::Move>(actions);
    ASSERT_NE(move, nullptr);
    EXPECT_FLOAT_EQ(move->position.x, 9.F);
    EXPECT_FLOAT_EQ(move->position.y, 2.F);
}

TEST_F(Inspector, InvalidPositionIsReported) {
    select("ship");
    const auto actions = replace_text(CLAY_ID("InspectorPositionY"), "abc");

    const auto* report = only<command::ReportInvalidInput>(actions);
    ASSERT_NE(report, nullptr);
    EXPECT_EQ(report->message, "Position: expected a number");
}

TEST_F(Inspector, EditingANumberFieldEmitsSetField) {
    select("ship");
    const auto actions = replace_text(CLAY_IDI("InspectorValue", index_of("Script.health")), "12");

    const auto* set = only_edit<edit::SetField>(actions);
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->key, "Script.health");
    EXPECT_EQ(set->value, FieldValue{std::int64_t{12}});
}

TEST_F(Inspector, InvalidNumberNamesTheField) {
    select("ship");
    const auto actions = replace_text(CLAY_IDI("InspectorValue", index_of("Script.health")), "x");

    const auto* report = only<command::ReportInvalidInput>(actions);
    ASSERT_NE(report, nullptr);
    EXPECT_EQ(report->message, "Script.health: expected a number");
}

TEST_F(Inspector, ClickingACheckboxEmitsTheOppositeValue) {
    select("rock");
    const auto actions = click(CLAY_IDI("InspectorValue", index_of("Script.armored")));

    const auto* set = only_edit<edit::SetField>(actions);
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->key, "Script.armored");
    EXPECT_EQ(set->value, FieldValue{true});
}

TEST_F(Inspector, EditingAVectorComponentKeepsTheOther) {
    select("rock");
    const auto actions = replace_text(CLAY_IDI("InspectorValueY", index_of("Script.drift")), "-2");

    const auto* set = only_edit<edit::SetField>(actions);
    ASSERT_NE(set, nullptr);
    const FieldValue expected{engine::core::Vec2{1.5F, -2.F}};
    EXPECT_EQ(set->value, expected);
}

TEST_F(Inspector, EditingAStringEmitsSetField) {
    select("rock");
    const auto actions = replace_text(CLAY_IDI("InspectorValue", index_of("Script.label")), "big");

    const auto* set = only_edit<edit::SetField>(actions);
    ASSERT_NE(set, nullptr);
    EXPECT_EQ(set->value, FieldValue{std::string("big")});
}

TEST_F(Inspector, EditingAColorAcceptsHex) {
    select("ship");
    const auto actions =
        replace_text(CLAY_IDI("InspectorValue", index_of("Sprite.color")), "#FF8000");

    const auto* set = only_edit<edit::SetField>(actions);
    ASSERT_NE(set, nullptr);
    const FieldValue expected{engine::core::Rgba{.r = 255, .g = 128, .b = 0, .a = 255}};
    EXPECT_EQ(set->value, expected);
}

TEST_F(Inspector, InvalidColorIsReported) {
    select("ship");
    const auto actions =
        replace_text(CLAY_IDI("InspectorValue", index_of("Sprite.color")), "orange");

    const auto* report = only<command::ReportInvalidInput>(actions);
    ASSERT_NE(report, nullptr);
    EXPECT_EQ(report->message, "Sprite.color: expected a color such as #FF8000 or #FF8000FF");
}

TEST_F(Inspector, ResetButtonResetsAnOverriddenField) {
    select("ship");
    const auto actions = click(CLAY_IDI("InspectorReset", index_of("Script.health")));

    const auto* reset = only_edit<edit::ResetField>(actions);
    ASSERT_NE(reset, nullptr);
    EXPECT_EQ(reset->key, "Script.health");
}

TEST_F(Inspector, OnlyOverriddenFieldsHaveAResetButton) {
    select("ship");
    frame();

    EXPECT_FALSE(Clay_GetElementData(CLAY_IDI("InspectorReset", index_of("Sprite.color"))).found);
}

TEST_F(Inspector, ResetAllEmitsResetAll) {
    select("ship");
    const auto actions = click(CLAY_ID("InspectorResetAll"));

    const auto* reset = only_edit<edit::ResetAll>(actions);
    ASSERT_NE(reset, nullptr);
    EXPECT_EQ(reset->id, id_of(_editor.session(), "ship"));
}

TEST_F(Inspector, NoResetAllWithoutOverrides) {
    select("rock");
    frame();

    EXPECT_FALSE(Clay_GetElementData(CLAY_ID("InspectorResetAll")).found);
}

TEST_F(Inspector, ShowsAnEditOnTheNextFrame) {
    select("ship");
    _editor.handle(edit::SetField{.id = id_of(_editor.session(), "ship"),
                                  .key = "Script.health",
                                  .value = FieldValue{std::int64_t{42}}});

    EXPECT_TRUE(shows("42"));
}

TEST_F(Inspector, EnterOnAnUntouchedFieldKeepsMoreDecimalsThanShown) {
    select("ship");
    const InstanceId ship = id_of(_editor.session(), "ship");
    _editor.handle(edit::Move{.id = ship, .position = engine::core::Vec2{16.875F, 2.F}});

    EXPECT_TRUE(press_enter_on(CLAY_ID("InspectorPositionX")).empty());
}

TEST_F(Inspector, OverriddenAndDefaultFieldsLineUp) {
    select("ship");
    frame();

    const float overridden = right_of(CLAY_IDI("InspectorValue", index_of("Script.health")));
    EXPECT_FLOAT_EQ(right_of(CLAY_IDI("InspectorValue", index_of("Sprite.color"))), overridden);
    EXPECT_FLOAT_EQ(right_of(CLAY_ID("InspectorName")), overridden);
    EXPECT_FLOAT_EQ(right_of(CLAY_ID("InspectorPositionY")), overridden);
}

TEST_F(Inspector, EmptyAssetShowsNone) {
    select("ship");
    _editor.handle(edit::SetField{.id = id_of(_editor.session(), "ship"),
                                  .key = "Sprite.path",
                                  .value = FieldValue{engine::core::AssetName{}}});

    EXPECT_TRUE(shows("none"));
}

TEST_F(Inspector, FieldsShowTheirValuesAfterLayout) {
    select("ship");
    frame();

    EXPECT_TRUE(shows("hero"));
    EXPECT_TRUE(shows("7"));
}

TEST_F(Inspector, LongValuesAreShortenedToFitTheirField) {
    select("ship");
    const std::string name(40, 'n');
    _editor.handle(edit::Rename{.id = id_of(_editor.session(), "ship"), .name = name});
    frame();
    const auto texts = frame().texts;

    EXPECT_EQ(std::ranges::count(texts, name), 1);
    EXPECT_TRUE(std::ranges::any_of(texts, [](const std::string& text) {
        return text.starts_with("nnn") && text.ends_with("...");
    }));
}

TEST_F(Inspector, FocusedFieldShowsTheWholeValue) {
    select("ship");
    const std::string name(40, 'n');
    _editor.handle(edit::Rename{.id = id_of(_editor.session(), "ship"), .name = name});
    click(CLAY_ID("InspectorName"));
    frame();

    std::ptrdiff_t letters = 0;
    for (const std::string& text : frame().texts) {
        if (!text.empty() && std::ranges::count(text, 'n') == std::ranges::ssize(text)) {
            letters += std::ranges::ssize(text);
        }
    }
    EXPECT_EQ(letters, 2 * std::ranges::ssize(name));
}

TEST_F(Inspector, PickingAnotherTypeEmitsChangeType) {
    select("ship");
    click(CLAY_ID("InspectorType"));
    frame();
    const auto actions = click(CLAY_IDI("DropdownOption", 0));

    const auto* change = only_edit<edit::ChangeType>(actions);
    ASSERT_NE(change, nullptr);
    EXPECT_EQ(change->id, id_of(_editor.session(), "ship"));
    EXPECT_EQ(change->type, "rock");
}

TEST_F(Inspector, ShowsTheScriptAndOpensIt) {
    select("rock");
    EXPECT_TRUE(shows("rock.lua"));

    const auto actions = click(CLAY_ID("InspectorOpenScript"));

    const auto* open = only<command::OpenScript>(actions);
    ASSERT_NE(open, nullptr);
    EXPECT_EQ(open->file, _game.path() / "scripts" / "rock.lua");
}

TEST_F(Inspector, ScriptNameIsCenteredOnTheOpenButton) {
    select("rock");
    frame();
    frame();

    const Clay_BoundingBox name = box_of(CLAY_ID("InspectorScriptName"));
    const Clay_BoundingBox open = box_of(CLAY_ID("InspectorOpenScript"));
    EXPECT_FLOAT_EQ(name.y + (name.height / 2.F), open.y + (open.height / 2.F));
}
