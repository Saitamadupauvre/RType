#include <gtest/gtest.h>

#include <array>
#include <string_view>

#include "ActionCodec.hpp"
#include "engine/script/InputState.hpp"

using engine::script::InputState;
using rtype::common::pack_input;
using rtype::common::PackedInput;
using rtype::common::unpack_input;

TEST(ActionCodec, NeutralInputPacksToZero) {
    EXPECT_EQ(pack_input(InputState{}), (PackedInput{.action_flags = 0, .move_x = 0, .move_y = 0}));
}

TEST(ActionCodec, FullAxesUseTheWholeRange) {
    const InputState state{.axes = {{"move_x", 1.F}, {"move_y", -1.F}}, .pressed = {}};

    EXPECT_EQ(pack_input(state), (PackedInput{.action_flags = 0, .move_x = 127, .move_y = -127}));
}

TEST(ActionCodec, OutOfRangeAxesAreClamped) {
    const InputState state{.axes = {{"move_x", 4.F}, {"move_y", -9.F}}, .pressed = {}};

    EXPECT_EQ(pack_input(state), (PackedInput{.action_flags = 0, .move_x = 127, .move_y = -127}));
}

TEST(ActionCodec, ButtonBitFollowsItsPositionInTheList) {
    constexpr std::array<std::string_view, 3> buttons{"fire", "bomb", "pause"};
    const InputState state{.axes = {}, .pressed = {"pause", "fire"}};

    EXPECT_EQ(pack_input(state, buttons).action_flags, 0b101U);
}

TEST(ActionCodec, UnknownPressedActionsAreNotSent) {
    const InputState state{.axes = {}, .pressed = {"move_up"}};

    EXPECT_EQ(pack_input(state).action_flags, 0U);
}

TEST(ActionCodec, UnpackRestoresAxesAndButtons) {
    const auto state = unpack_input(PackedInput{.action_flags = 1, .move_x = 127, .move_y = -127});

    EXPECT_FLOAT_EQ(state.axes.at("move_x"), 1.F);
    EXPECT_FLOAT_EQ(state.axes.at("move_y"), -1.F);
    EXPECT_TRUE(state.pressed.contains("fire"));
}

TEST(ActionCodec, UnpackClampsTheMostNegativeAxis) {
    const auto state = unpack_input(PackedInput{.action_flags = 0, .move_x = -128, .move_y = 0});

    EXPECT_FLOAT_EQ(state.axes.at("move_x"), -1.F);
}

TEST(ActionCodec, UnpackIgnoresBitsWithoutAnAction) {
    const auto state = unpack_input(PackedInput{.action_flags = 0xFE, .move_x = 0, .move_y = 0});

    EXPECT_TRUE(state.pressed.empty());
}

TEST(ActionCodec, RoundTripKeepsPressedButtons) {
    const InputState state{.axes = {{"move_x", 0.F}, {"move_y", 0.F}}, .pressed = {"fire"}};

    EXPECT_EQ(unpack_input(pack_input(state)).pressed, state.pressed);
}
