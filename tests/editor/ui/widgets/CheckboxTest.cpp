#include <gtest/gtest.h>

#include "WidgetFixture.hpp"
#include "ui/widgets/Checkbox.hpp"

namespace {

class Checkbox : public WidgetFixture {
protected:
    bool _value = false;

    static Clay_ElementId id() { return CLAY_ID("Check"); }

    bool step(const editor::UiInput& input = {}) {
        bool toggled = false;
        frame(input, [this, &toggled](editor::Ui& ui) {
            toggled = editor::checkbox(ui, id(), _value, CLAY_STRING("Visible"));
        });
        return toggled;
    }
};

} // namespace

TEST_F(Checkbox, ClickReportsAToggle) {
    step();
    EXPECT_TRUE(step(click_at(id())));
}

TEST_F(Checkbox, ClickOnTheLabelReportsAToggle) {
    step();
    const Clay_BoundingBox box = Clay_GetElementData(id()).boundingBox;
    EXPECT_TRUE(step(click_at(id(), box.width - 2.F)));
}

TEST_F(Checkbox, ClickOutsideReportsNothing) {
    step();
    EXPECT_FALSE(step(click_outside()));
}

TEST_F(Checkbox, NeverTakesTheKeyboard) {
    step();
    step(click_at(id()));
    step();
    EXPECT_FALSE(_widgets.wants_keyboard());
}
