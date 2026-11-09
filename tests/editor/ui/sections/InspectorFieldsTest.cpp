#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "../widgets/WidgetFixture.hpp"
#include "action/Action.hpp"
#include "action/InspectorModel.hpp"
#include "ui/UiInput.hpp"
#include "ui/sections/InspectorFields.hpp"

using editor::InspectorField;
using editor::Key;
using engine::core::AssetName;
using engine::core::FieldKind;
using engine::core::FieldList;
using engine::core::FieldValue;
using engine::core::ScalarValue;

namespace edit = editor::edit;

namespace {

class InspectorFields : public WidgetFixture {
protected:
    InspectorField _field{.key = "Tags.labels",
                          .kind = FieldKind::List,
                          .value = FieldValue{FieldList{.element = FieldKind::String,
                                                        .items = {ScalarValue{std::string("a")},
                                                                  ScalarValue{std::string("b")}}}},
                          .overridden = false};
    std::vector<editor::Action> _actions;

    void step(const editor::UiInput& input = {}) {
        frame(input, [this](editor::Ui& ui) {
            editor::field_row(ui, {.id = {}, .field = _field, .index = 0, .label = "labels"});
            for (auto& action : ui.take_actions()) {
                _actions.push_back(std::move(action));
            }
        });
    }

    void click(Clay_ElementId id) {
        step();
        _actions.clear();
        step(click_at(id));
    }

    [[nodiscard]] const FieldValue* set_value() const {
        if (_actions.size() != 1) {
            return nullptr;
        }
        const auto* change = std::get_if<editor::Edit>(&_actions.front());
        const auto* set = change ? std::get_if<edit::SetField>(change) : nullptr;
        return set ? &set->value : nullptr;
    }

    static FieldValue strings(std::vector<std::string> items) {
        FieldList list{.element = FieldKind::String, .items = {}};
        for (std::string& item : items) {
            list.items.emplace_back(std::move(item));
        }
        return FieldValue{std::move(list)};
    }
};

} // namespace

TEST_F(InspectorFields, AddAppendsTheDefaultItemOfTheListKind) {
    click(CLAY_IDI("InspectorItemAdd", 0));

    ASSERT_NE(set_value(), nullptr);
    EXPECT_EQ(*set_value(), strings({"a", "b", ""}));
}

TEST_F(InspectorFields, AddToAnIntListAppendsZero) {
    _field.value = FieldValue{FieldList{.element = FieldKind::Int, .items = {}}};
    click(CLAY_IDI("InspectorItemAdd", 0));

    ASSERT_NE(set_value(), nullptr);
    const FieldValue expected{
        FieldList{.element = FieldKind::Int, .items = {ScalarValue{std::int64_t{0}}}}};
    EXPECT_EQ(*set_value(), expected);
}

TEST_F(InspectorFields, RemoveDropsThatItem) {
    click(CLAY_IDI("InspectorItemRemove", 1));

    ASSERT_NE(set_value(), nullptr);
    EXPECT_EQ(*set_value(), strings({"a"}));
}

TEST_F(InspectorFields, EditingAnItemReplacesIt) {
    click(CLAY_IDI("InspectorItem", 0));
    step();
    step(press(Key::a, true));
    step(type("z"));
    _actions.clear();
    step(press(Key::enter));

    ASSERT_NE(set_value(), nullptr);
    EXPECT_EQ(*set_value(), strings({"z", "b"}));
}

TEST_F(InspectorFields, AssetFieldEditsItsPath) {
    _field = {.key = "Sprite.path",
              .kind = FieldKind::Asset,
              .value = FieldValue{AssetName{.path = "ship.png"}},
              .overridden = false};
    click(CLAY_IDI("InspectorValue", 0));
    step();
    step(press(Key::a, true));
    step(type("enemy.png"));
    _actions.clear();
    step(press(Key::enter));

    ASSERT_NE(set_value(), nullptr);
    EXPECT_EQ(*set_value(), FieldValue{AssetName{.path = "enemy.png"}});
}
