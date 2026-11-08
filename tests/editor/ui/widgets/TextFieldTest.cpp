#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "WidgetFixture.hpp"
#include "ui/UiInput.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/TextField.hpp"

using editor::FieldEvent;
using editor::Key;
using editor::TextResult;
using editor::UiInput;

namespace {

constexpr float padding = 6.F;
constexpr float byte_width = 8.F;

class TextField : public WidgetFixture {
protected:
    std::string _first = "abc";
    std::string _second = "xyz";
    TextResult _first_result;
    TextResult _second_result;
    editor::TextFieldOptions _first_options;
    bool _draws_second = true;

    static Clay_ElementId first_id() { return CLAY_ID("First"); }
    static Clay_ElementId second_id() { return CLAY_ID("Second"); }

    void step(const UiInput& input = {}) {
        frame(input, [this](editor::Ui& ui) {
            _first_result = editor::text_field(ui, first_id(), _first, _first_options);
            if (_draws_second) {
                _second_result = editor::text_field(ui, second_id(), _second);
            }
        });
        commit(_first_result, _first);
        commit(_second_result, _second);
    }

    void focus_first(float x_from_left = 1.F) {
        step();
        step(click_at(first_id(), x_from_left));
        step();
    }

private:
    static void commit(const TextResult& result, std::string& value) {
        if (result.event == FieldEvent::committed) {
            value = result.value;
        }
    }
};

} // namespace

TEST_F(TextField, ClickFocusesOnTheNextFrame) {
    step();
    step(click_at(first_id()));
    EXPECT_FALSE(_widgets.is_focused(first_id()));
    step();
    EXPECT_TRUE(_widgets.is_focused(first_id()));
}

TEST_F(TextField, WantsKeyboardOnlyWhileFocused) {
    step();
    EXPECT_FALSE(_widgets.wants_keyboard());
    focus_first();
    EXPECT_TRUE(_widgets.wants_keyboard());
    step(press(Key::enter));
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(TextField, ClickPlacesTheCaretUnderThePointer) {
    focus_first(padding + (2 * byte_width));
    step(type("X"));
    EXPECT_EQ(_first_result.value, "abXc");
}

TEST_F(TextField, TypingChangesTheDraftOnly) {
    focus_first(100.F);
    step(type("d"));
    EXPECT_EQ(_first_result.event, FieldEvent::changed);
    EXPECT_EQ(_first_result.value, "abcd");
    EXPECT_EQ(_first, "abc");
}

TEST_F(TextField, EnterCommitsTheDraft) {
    focus_first(100.F);
    step(type("d"));
    step(press(Key::enter));
    EXPECT_EQ(_first_result.event, FieldEvent::committed);
    EXPECT_EQ(_first, "abcd");
}

TEST_F(TextField, EscRestoresTheValue) {
    focus_first(100.F);
    step(type("d"));
    step(press(Key::escape));
    EXPECT_EQ(_first_result.event, FieldEvent::cancelled);
    EXPECT_EQ(_first_result.value, "abc");
    EXPECT_EQ(_first, "abc");
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(TextField, ClickOutsideCommitsAndBlurs) {
    focus_first(100.F);
    step(type("d"));
    step(click_outside());
    EXPECT_EQ(_first_result.event, FieldEvent::committed);
    EXPECT_EQ(_first, "abcd");
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(TextField, ClickOnAnotherFieldCommitsThenMovesFocus) {
    focus_first(100.F);
    step(type("d"));
    step(click_at(second_id()));
    EXPECT_EQ(_first, "abcd");
    step();
    EXPECT_TRUE(_widgets.is_focused(second_id()));
}

TEST_F(TextField, TabCommitsAndFocusesTheNextFieldWithItsTextSelected) {
    focus_first(100.F);
    step(type("d"));
    step(press(Key::tab));
    EXPECT_EQ(_first, "abcd");
    step();
    EXPECT_TRUE(_widgets.is_focused(second_id()));
    EXPECT_EQ(_widgets.draft().selection(), "xyz");
}

TEST_F(TextField, TabWrapsFromTheLastFieldToTheFirst) {
    focus_first();
    step(press(Key::tab));
    step();
    step(press(Key::tab));
    step();
    EXPECT_TRUE(_widgets.is_focused(first_id()));
}

TEST_F(TextField, ShiftTabFocusesThePreviousField) {
    focus_first();
    step(press(Key::tab, false, true));
    step();
    EXPECT_TRUE(_widgets.is_focused(second_id()));
}

TEST_F(TextField, ReadOnlyFieldNeverTakesFocus) {
    _first_options.read_only = true;
    focus_first();
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(TextField, ReadOnlyFieldIsSkippedByTab) {
    _first_options.read_only = true;
    step();
    step(click_at(second_id()));
    step();
    step(press(Key::tab));
    step();
    EXPECT_TRUE(_widgets.is_focused(second_id()));
}

TEST_F(TextField, FieldThatIsNoLongerDrawnLosesFocus) {
    step();
    step(click_at(second_id()));
    step();
    _draws_second = false;
    step();
    EXPECT_FALSE(_widgets.wants_keyboard());
}

TEST_F(TextField, CopyAndPasteGoThroughTheClipboard) {
    focus_first();
    step(press(Key::a, true));
    step(press(Key::c, true));
    EXPECT_EQ(_clipboard, "abc");
    step(press(Key::end));
    step(press(Key::v, true));
    EXPECT_EQ(_first_result.value, "abcabc");
}

TEST_F(TextField, CutRemovesTheSelection) {
    focus_first();
    step(press(Key::a, true));
    step(press(Key::x, true));
    EXPECT_EQ(_clipboard, "abc");
    EXPECT_EQ(_first_result.value, "");
}

TEST_F(TextField, PasteReplacesTheSelection) {
    _clipboard = "new";
    focus_first();
    step(press(Key::a, true));
    step(press(Key::v, true));
    EXPECT_EQ(_first_result.value, "new");
}

TEST_F(TextField, TypedTextIsIgnoredWithThePrimaryModifier) {
    focus_first(100.F);
    step(UiInput{.typed = "q", .primary_modifier = true});
    EXPECT_EQ(_first_result.event, FieldEvent::none);
}

TEST_F(TextField, BackspaceErasesBeforeTheCaret) {
    focus_first(100.F);
    step(press(Key::backspace));
    EXPECT_EQ(_first_result.value, "ab");
}

TEST_F(TextField, RepeatedKeysKeepEditing) {
    focus_first(100.F);
    step(UiInput{.keys_repeated = {Key::backspace}});
    step(UiInput{.keys_repeated = {Key::backspace}});
    EXPECT_EQ(_first_result.value, "a");
}

TEST_F(TextField, CtrlBackspaceErasesTheWordBeforeTheCaret) {
    _first = "one two";
    focus_first(100.F);
    step(press(Key::backspace, true));
    EXPECT_EQ(_first_result.value, "one ");
}

TEST_F(TextField, CtrlDeleteErasesTheWordAfterTheCaret) {
    _first = "one two";
    focus_first();
    step(press(Key::del, true));
    EXPECT_EQ(_first_result.value, " two");
}

TEST_F(TextField, CtrlShiftArrowSelectsByWord) {
    _first = "one two";
    focus_first(100.F);
    step(press(Key::left, true, true));
    EXPECT_EQ(_widgets.draft().selection(), "two");
}

TEST_F(TextField, DoubleClickSelectsTheWordUnderThePointer) {
    _first = "one two";
    focus_first();
    UiInput second = click_at(first_id(), padding + (5 * byte_width));
    second.click_count = 2;
    step(second);
    EXPECT_EQ(_widgets.draft().selection(), "two");
}

TEST_F(TextField, TripleClickSelectsEverything) {
    _first = "one two";
    focus_first();
    UiInput third = click_at(first_id(), padding + byte_width);
    third.click_count = 3;
    step(third);
    EXPECT_EQ(_widgets.draft().selection(), "one two");
}

TEST_F(TextField, DragExtendsTheSelectionFromThePress) {
    focus_first();
    step(click_at(first_id(), padding));
    UiInput drag = click_at(first_id(), padding + (2 * byte_width));
    drag.primary_pressed = false;
    step(drag);
    EXPECT_EQ(_widgets.draft().selection(), "ab");
}

TEST_F(TextField, DragStartedByTheFocusingClickSelects) {
    step();
    step(click_at(first_id(), padding));
    UiInput drag = click_at(first_id(), padding + (3 * byte_width));
    drag.primary_pressed = false;
    step(drag);
    EXPECT_EQ(_widgets.draft().selection(), "abc");
}

TEST_F(TextField, MovingWithTheButtonUpDoesNotSelect) {
    focus_first();
    step(click_at(first_id(), padding));
    step();
    step({.pointer = click_at(first_id(), padding + (2 * byte_width)).pointer});
    EXPECT_FALSE(_widgets.draft().has_selection());
}

TEST_F(TextField, LongTextScrollsToKeepTheCaretVisible) {
    _first = std::string(200, 'a');
    focus_first();
    step(press(Key::end));
    step();

    const float visible = window.width - (2 * padding) - 1.F;
    EXPECT_FLOAT_EQ(_widgets.text_offset(), (200 * byte_width) - visible);
}

TEST_F(TextField, HomeScrollsBackToTheStart) {
    _first = std::string(200, 'a');
    focus_first();
    step(press(Key::end));
    step(press(Key::home));
    step();

    EXPECT_FLOAT_EQ(_widgets.text_offset(), 0.F);
}

TEST_F(TextField, ClickInAScrolledFieldPlacesTheCaretUnderThePointer) {
    _first = std::string(200, 'a');
    focus_first();
    step(press(Key::end));
    step();
    const float offset = _widgets.text_offset();

    step(click_at(first_id(), padding + (4 * byte_width)));
    step(type("X"));

    const auto expected = static_cast<std::size_t>((offset / byte_width) + 4.5F);
    EXPECT_EQ(_first_result.value.find('X'), expected);
}

TEST_F(TextField, FocusStartsUnscrolled) {
    _first = std::string(200, 'a');
    focus_first();
    step(press(Key::end));
    step(press(Key::enter));
    focus_first();

    EXPECT_FLOAT_EQ(_widgets.text_offset(), 0.F);
}
