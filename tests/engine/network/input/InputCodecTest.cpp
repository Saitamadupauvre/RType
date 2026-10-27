#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <optional>

#include "engine/core/InputState.hpp"
#include "engine/core/Keys.hpp"
#include "input/InputCodec.hpp"
#include "protocol/Messages.hpp"

using engine::core::InputState;
using engine::core::Key;
using engine::core::MouseButton;
using engine::core::Vec2;
using engine::network::InputPayload;
using engine::network::PointerPosition;
using engine::network::to_input_state;
using engine::network::to_payload;

TEST(InputCodec, IdleInputIsAllZero) {
    EXPECT_EQ(to_payload(InputState{}), InputPayload{});
    EXPECT_EQ(to_input_state(InputPayload{}), InputState{});
}

TEST(InputCodec, KeyIsTheBitOfItsValueLowestBitFirst) {
    const auto payload =
        to_payload(InputState{.keys = {Key::A, Key::H}, .mouse_buttons = {}, .pointer = {}});

    InputPayload expected{};
    expected.keys[0] = 0x02;
    expected.keys[1] = 0x01;
    EXPECT_EQ(payload, expected);
}

TEST(InputCodec, MouseButtonIsTheBitOfItsValue) {
    const auto payload = to_payload(InputState{
        .keys = {}, .mouse_buttons = {MouseButton::Left, MouseButton::Middle}, .pointer = {}});

    EXPECT_EQ(payload.mouse_buttons, 0x05);
}

TEST(InputCodec, NeverSendsTheUnknownKey) {
    EXPECT_EQ(to_payload(InputState{.keys = {Key::Unknown}, .mouse_buttons = {}, .pointer = {}}),
              InputPayload{});
}

TEST(InputCodec, PointerIsRoundedAndClampedToWireCoordinates) {
    const auto payload =
        to_payload(InputState{.keys = {}, .mouse_buttons = {}, .pointer = Vec2{10.6F, -1e9F}});

    EXPECT_EQ(payload.pointer, (PointerPosition{.x = 11, .y = -32768}));
}

TEST(InputCodec, NanPointerCoordinateIsSentAsZero) {
    const auto payload =
        to_payload(InputState{.keys = {},
                              .mouse_buttons = {},
                              .pointer = Vec2{std::numeric_limits<float>::quiet_NaN(), 4.F}});

    EXPECT_EQ(payload.pointer, (PointerPosition{.x = 0, .y = 4}));
}

TEST(InputCodec, RoundTripsEveryKeyAndButton) {
    InputState every;
    for (const auto& info : engine::core::keys) {
        every.keys.insert(info.key);
    }
    for (const auto& info : engine::core::mouse_buttons) {
        every.mouse_buttons.insert(info.button);
    }
    every.pointer = Vec2{-3.F, 700.F};

    EXPECT_EQ(to_input_state(to_payload(every)), every);
}

TEST(InputCodec, DecodingIgnoresBitsOutsideTheEnumerations) {
    InputPayload payload{};
    payload.keys[0] = 0x01;
    payload.keys[15] = 0x80;
    payload.mouse_buttons = 0x80;

    EXPECT_EQ(to_input_state(payload), InputState{});
}
