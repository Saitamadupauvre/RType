#include <gtest/gtest.h>

#include "TemporaryNetworkView.hpp"
#include "engine/script/InputState.hpp"

using engine::script::InputState;
using rtype::client::temporary_to_network_input;
using rtype::client::TemporaryNetworkInput;

TEST(TemporaryNetworkView, NeutralControlsSendNothing) {
    EXPECT_EQ(temporary_to_network_input(InputState{}),
              (TemporaryNetworkInput{.action_flags = 0, .move_x = 0, .move_y = 0}));
}

TEST(TemporaryNetworkView, FullAxesUseTheWholeRange) {
    const InputState state{.axes = {{"move_x", 1.F}, {"move_y", -1.F}}, .pressed = {}};

    EXPECT_EQ(temporary_to_network_input(state),
              (TemporaryNetworkInput{.action_flags = 0, .move_x = 127, .move_y = -127}));
}

TEST(TemporaryNetworkView, OutOfRangeAxesAreClamped) {
    const InputState state{.axes = {{"move_x", 4.F}, {"move_y", -9.F}}, .pressed = {}};

    EXPECT_EQ(temporary_to_network_input(state),
              (TemporaryNetworkInput{.action_flags = 0, .move_x = 127, .move_y = -127}));
}

TEST(TemporaryNetworkView, FireSetsTheFireFlag) {
    const InputState state{.axes = {}, .pressed = {"fire"}};

    EXPECT_EQ(temporary_to_network_input(state).action_flags, 1U);
}
