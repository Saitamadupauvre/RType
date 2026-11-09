#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "WidgetFixture.hpp"
#include "ui/UiInput.hpp"
#include "ui/widgets/Dropdown.hpp"

using editor::Key;

namespace {

constexpr std::array<std::string_view, 3> layers{"Background", "Gameplay", "Foreground"};

class Dropdown : public WidgetFixture {
protected:
    std::size_t _selected = 1;
    std::optional<std::size_t> _picked;
    bool _draws_options = true;

    static Clay_ElementId id() { return CLAY_ID("Layer"); }
    static Clay_ElementId option(std::uint32_t index) { return CLAY_IDI("DropdownOption", index); }

    void step(const editor::UiInput& input = {}) {
        frame(input, [this](editor::Ui& ui) {
            const std::span<const std::string_view> options =
                _draws_options ? std::span<const std::string_view>(layers)
                               : std::span<const std::string_view>();
            _picked = editor::dropdown(ui, id(), _selected, options);
        });
    }

    void open() {
        step();
        step(click_at(id()));
        step();
    }
};

} // namespace

TEST_F(Dropdown, ClickOpensTheList) {
    open();
    EXPECT_TRUE(_widgets.is_open(id()));
}

TEST_F(Dropdown, OpensWithTheSelectedOptionHighlighted) {
    open();
    EXPECT_EQ(_widgets.highlighted(), 1U);
}

TEST_F(Dropdown, ClickOnAnOptionPicksItAndCloses) {
    open();
    step(click_at(option(2)));
    EXPECT_EQ(_picked, 2U);
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, ListFloatsBelowTheButton) {
    open();
    const Clay_BoundingBox button = Clay_GetElementData(id()).boundingBox;
    const Clay_BoundingBox first = Clay_GetElementData(option(0)).boundingBox;
    EXPECT_GE(first.y, button.y + button.height);
}

TEST_F(Dropdown, ClickOutsideClosesWithoutPicking) {
    open();
    step(click_outside());
    EXPECT_FALSE(_picked.has_value());
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, ClickOnTheButtonClosesAnOpenList) {
    open();
    step(click_at(id()));
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, ArrowsMoveTheHighlightWithinTheList) {
    open();
    step(press(Key::down));
    step(press(Key::down));
    EXPECT_EQ(_widgets.highlighted(), 2U);
    step(press(Key::up));
    step(press(Key::up));
    step(press(Key::up));
    EXPECT_EQ(_widgets.highlighted(), 0U);
}

TEST_F(Dropdown, EnterPicksTheHighlightedOption) {
    open();
    step(press(Key::down));
    step(press(Key::enter));
    EXPECT_EQ(_picked, 2U);
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, EscClosesWithoutPicking) {
    open();
    step(press(Key::escape));
    EXPECT_FALSE(_picked.has_value());
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, KeysDoNothingWhileClosed) {
    step();
    step(press(Key::enter));
    EXPECT_FALSE(_picked.has_value());
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, WithoutOptionsItNeverOpens) {
    _draws_options = false;
    open();
    EXPECT_FALSE(_widgets.is_open(id()));
}

TEST_F(Dropdown, OpenListDoesNotTakeTheKeyboard) {
    open();
    EXPECT_FALSE(_widgets.wants_keyboard());
}
