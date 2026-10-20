#include <gtest/gtest.h>

#include <initializer_list>
#include <set>

#include "Controls.hpp"
#include "engine/graphics/IInput.hpp"

using engine::graphics::Key;
using rtype::client::read_controls;

namespace {

class FakeInput final : public engine::graphics::IInput {
public:
    FakeInput(std::initializer_list<Key> held) : _held(held) {}

    [[nodiscard]] bool is_key_down(Key key) const override { return _held.contains(key); }
    [[nodiscard]] bool is_key_pressed(Key) const override { return false; }
    [[nodiscard]] bool is_key_released(Key) const override { return false; }

private:
    std::set<Key> _held;
};

} // namespace

TEST(Controls, NoKeyIsNeutral) {
    const auto state = read_controls(FakeInput{});
    EXPECT_FLOAT_EQ(state.axes.at("move_x"), 0.F);
    EXPECT_FLOAT_EQ(state.axes.at("move_y"), 0.F);
    EXPECT_TRUE(state.pressed.empty());
}

TEST(Controls, ArrowsGiveAxes) {
    const auto state = read_controls(FakeInput{Key::Right, Key::Up});
    EXPECT_FLOAT_EQ(state.axes.at("move_x"), 1.F);
    EXPECT_FLOAT_EQ(state.axes.at("move_y"), -1.F);
}

TEST(Controls, AlternateKeysGiveSameAxes) {
    const auto state = read_controls(FakeInput{Key::Q, Key::S});
    EXPECT_FLOAT_EQ(state.axes.at("move_x"), -1.F);
    EXPECT_FLOAT_EQ(state.axes.at("move_y"), 1.F);
}

TEST(Controls, OppositeKeysCancel) {
    const auto state = read_controls(FakeInput{Key::Left, Key::D});
    EXPECT_FLOAT_EQ(state.axes.at("move_x"), 0.F);
}

TEST(Controls, HeldSpaceIsFire) {
    const auto state = read_controls(FakeInput{Key::Space});
    EXPECT_TRUE(state.pressed.contains("fire"));
}

TEST(Controls, UnboundKeyDoesNothing) {
    const auto state = read_controls(FakeInput{Key::Enter});
    EXPECT_FLOAT_EQ(state.axes.at("move_x"), 0.F);
    EXPECT_TRUE(state.pressed.empty());
}
