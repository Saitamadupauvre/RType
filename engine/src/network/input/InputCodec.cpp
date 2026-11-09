#include "InputCodec.hpp"

#include <cstddef>
#include <cstdint>

#include "protocol/FieldWire.hpp"

namespace engine::network {

namespace {

std::uint8_t bit_of(std::size_t index) noexcept {
    return static_cast<std::uint8_t>(1U << (index % 8));
}

bool has_bit(std::uint8_t byte, std::size_t index) noexcept { return (byte & bit_of(index)) != 0; }

} // namespace

InputPayload to_payload(const core::InputState& input) {
    InputPayload payload{};
    for (const core::Key key : input.keys) {
        const auto index = static_cast<std::size_t>(key);
        if (key != core::Key::Unknown && index < core::key_count) {
            payload.keys.at(index / 8) |= bit_of(index);
        }
    }
    for (const core::MouseButton button : input.mouse_buttons) {
        const auto index = static_cast<std::size_t>(button);
        if (index < core::mouse_button_count) {
            payload.mouse_buttons |= bit_of(index);
        }
    }
    if (input.pointer) {
        payload.pointer = PointerPosition{.x = to_wire_coordinate(input.pointer->x),
                                          .y = to_wire_coordinate(input.pointer->y)};
    }
    return payload;
}

core::InputState to_input_state(const InputPayload& payload) {
    core::InputState input;
    for (const core::KeyInfo& info : core::keys) {
        const auto index = static_cast<std::size_t>(info.key);
        if (has_bit(payload.keys.at(index / 8), index)) {
            input.keys.insert(info.key);
        }
    }
    for (const core::MouseButtonInfo& info : core::mouse_buttons) {
        if (has_bit(payload.mouse_buttons, static_cast<std::size_t>(info.button))) {
            input.mouse_buttons.insert(info.button);
        }
    }
    if (payload.pointer) {
        input.pointer = core::Vec2{static_cast<float>(payload.pointer->x),
                                   static_cast<float>(payload.pointer->y)};
    }
    return input;
}

} // namespace engine::network
