#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>

#include "WidgetFixture.hpp"
#include "ui/UiInput.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/NumberField.hpp"

using editor::FieldEvent;
using editor::FieldResult;
using editor::Key;

namespace {

class NumberField : public WidgetFixture {
protected:
    float _value = 1.5F;
    int _count = 3;
    std::int64_t _wide = 5;
    FieldResult<float> _value_result;
    FieldResult<int> _count_result;
    FieldResult<std::int64_t> _wide_result;
    editor::NumberFieldOptions _options;

    static Clay_ElementId value_id() { return CLAY_ID("Value"); }
    static Clay_ElementId count_id() { return CLAY_ID("Count"); }
    static Clay_ElementId wide_id() { return CLAY_ID("Wide"); }

    void step(const editor::UiInput& input = {}) {
        frame(input, [this](editor::Ui& ui) {
            _value_result = editor::number_field(ui, value_id(), _value, _options);
            _count_result = editor::number_field(ui, count_id(), _count, _options);
            _wide_result = editor::number_field(ui, wide_id(), _wide, _options);
        });
    }

    void enter(Clay_ElementId id, const std::string& text) {
        step();
        step(click_at(id));
        step();
        step(press(Key::a, true));
        step(type(text));
        step(press(Key::enter));
    }
};

} // namespace

TEST_F(NumberField, ShowsTheValueWithItsDecimals) {
    _options.decimals = 3;
    step();
    step(click_at(value_id()));
    step();
    EXPECT_EQ(_widgets.draft().text(), "1.500");
}

TEST_F(NumberField, CommitsAValidNumber) {
    enter(value_id(), "2.25");
    EXPECT_EQ(_value_result.event, FieldEvent::committed);
    EXPECT_FLOAT_EQ(_value_result.value, 2.25F);
}

TEST_F(NumberField, TypingKeepsTheValueUntilCommit) {
    step();
    step(click_at(value_id()));
    step();
    step(type("9"));
    EXPECT_EQ(_value_result.event, FieldEvent::changed);
    EXPECT_FLOAT_EQ(_value_result.value, 1.5F);
}

TEST_F(NumberField, InvalidInputIsReverted) {
    enter(value_id(), "abc");
    EXPECT_EQ(_value_result.event, FieldEvent::rejected);
    EXPECT_FLOAT_EQ(_value_result.value, 1.5F);
}

TEST_F(NumberField, TrailingGarbageIsReverted) {
    enter(value_id(), "3x");
    EXPECT_EQ(_value_result.event, FieldEvent::rejected);
}

TEST_F(NumberField, EmptyInputIsReverted) {
    step();
    step(click_at(value_id()));
    step();
    step(press(Key::a, true));
    step(press(Key::backspace));
    step(press(Key::enter));
    EXPECT_EQ(_value_result.event, FieldEvent::rejected);
}

TEST_F(NumberField, OutOfRangeDoubleIsReverted) {
    enter(value_id(), "1e400");
    EXPECT_EQ(_value_result.event, FieldEvent::rejected);
}

TEST_F(NumberField, NotANumberIsReverted) {
    enter(value_id(), "nan");
    EXPECT_EQ(_value_result.event, FieldEvent::rejected);
}

TEST_F(NumberField, ValueIsClampedToTheBounds) {
    _options.min = 0.0;
    _options.max = 10.0;
    enter(value_id(), "50");
    EXPECT_FLOAT_EQ(_value_result.value, 10.F);
    enter(value_id(), "-4");
    EXPECT_FLOAT_EQ(_value_result.value, 0.F);
}

TEST_F(NumberField, FloatBeyondItsRangeIsClampedNotInfinite) {
    enter(value_id(), "1e300");
    EXPECT_EQ(_value_result.event, FieldEvent::committed);
    EXPECT_FLOAT_EQ(_value_result.value, std::numeric_limits<float>::max());
}

TEST_F(NumberField, IntegerIsRounded) {
    enter(count_id(), "4.6");
    EXPECT_EQ(_count_result.event, FieldEvent::committed);
    EXPECT_EQ(_count_result.value, 5);
}

TEST_F(NumberField, IntegerBeyondItsRangeIsClamped) {
    enter(count_id(), "1e12");
    EXPECT_EQ(_count_result.value, std::numeric_limits<int>::max());
}

TEST_F(NumberField, EscRevertsWithTheValue) {
    step();
    step(click_at(count_id()));
    step();
    step(type("7"));
    step(press(Key::escape));
    EXPECT_EQ(_count_result.event, FieldEvent::cancelled);
    EXPECT_EQ(_count_result.value, 3);
}

TEST_F(NumberField, WideIntegerCommitsBeyondTheIntRange) {
    enter(wide_id(), "5000000000");
    EXPECT_EQ(_wide_result.event, FieldEvent::committed);
    EXPECT_EQ(_wide_result.value, 5'000'000'000);
}

TEST_F(NumberField, WideIntegerAtTheTopOfItsRangeIsClampedWithoutOverflow) {
    enter(wide_id(), "1e30");
    EXPECT_EQ(_wide_result.value, std::numeric_limits<std::int64_t>::max());
    enter(wide_id(), "-1e30");
    EXPECT_EQ(_wide_result.value, std::numeric_limits<std::int64_t>::min());
}

TEST_F(NumberField, CommittingUntouchedTextKeepsTheExactValue) {
    _value = 16.875F;
    step();
    step(click_at(value_id()));
    step();
    step(press(Key::enter));
    EXPECT_EQ(_value_result.event, FieldEvent::committed);
    EXPECT_EQ(_value_result.value, 16.875F);
}
