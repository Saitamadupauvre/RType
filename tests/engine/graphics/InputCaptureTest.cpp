#include <gtest/gtest.h>

#include <set>
#include <utility>

#include "engine/core/InputState.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/InputCapture.hpp"

using engine::core::Vec2;
using engine::graphics::capture_input;
using engine::graphics::Key;
using engine::graphics::MouseButton;

namespace {

class FakeInput final : public engine::graphics::IInput {
public:
    FakeInput(std::set<Key> keys, std::set<MouseButton> buttons, Vec2 mouse)
        : _keys(std::move(keys)), _buttons(std::move(buttons)), _mouse(mouse) {}

    [[nodiscard]] bool is_key_down(Key key) const override { return _keys.contains(key); }
    [[nodiscard]] bool is_key_pressed(Key) const override { return false; }
    [[nodiscard]] bool is_key_released(Key) const override { return false; }
    [[nodiscard]] Key last_key_pressed() override { return Key::Unknown; }
    [[nodiscard]] Vec2 mouse_position() const override { return _mouse; }
    [[nodiscard]] bool is_mouse_down(MouseButton button) const override {
        return _buttons.contains(button);
    }
    [[nodiscard]] bool is_mouse_pressed(MouseButton) const override { return false; }
    [[nodiscard]] bool is_mouse_released(MouseButton) const override { return false; }

private:
    std::set<Key> _keys;
    std::set<MouseButton> _buttons;
    Vec2 _mouse;
};

} // namespace

TEST(InputCapture, NothingHeldGivesOnlyThePointer) {
    const auto state = capture_input(FakeInput({}, {}, Vec2{3.F, 4.F}));

    EXPECT_TRUE(state.keys.empty());
    EXPECT_TRUE(state.mouse_buttons.empty());
    EXPECT_EQ(state.pointer, (Vec2{3.F, 4.F}));
}

TEST(InputCapture, ReadsEveryHeldKeyAndButton) {
    const auto state = capture_input(
        FakeInput({Key::Space, Key::Left, Key::GraveAccent}, {MouseButton::Right}, Vec2{}));

    EXPECT_EQ(state.keys, (std::set<Key>{Key::Space, Key::Left, Key::GraveAccent}));
    EXPECT_EQ(state.mouse_buttons, std::set<MouseButton>{MouseButton::Right});
}

TEST(InputCapture, NeverReportsTheUnknownKey) {
    const auto state = capture_input(FakeInput({Key::Unknown}, {}, Vec2{}));

    EXPECT_TRUE(state.keys.empty());
}
