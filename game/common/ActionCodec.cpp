#include "ActionCodec.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace rtype::common {

namespace {

constexpr std::size_t max_buttons = 8;
constexpr std::string_view move_x = "move_x";
constexpr std::string_view move_y = "move_y";

std::int8_t pack_axis(const engine::script::InputState& state, std::string_view axis) {
    const auto found = state.axes.find(axis);
    if (found == state.axes.end()) {
        return 0;
    }
    return static_cast<std::int8_t>(std::lround(std::clamp(found->second, -1.F, 1.F)));
}

float unpack_axis(std::int8_t value) { return std::clamp(static_cast<float>(value), -1.F, 1.F); }

std::size_t usable(std::span<const std::string_view> buttons) {
    return std::min(buttons.size(), max_buttons);
}

} // namespace

PackedInput pack_input(const engine::script::InputState& state,
                       std::span<const std::string_view> buttons) {
    std::uint8_t flags = 0;
    for (std::size_t bit = 0; bit < usable(buttons); ++bit) {
        if (state.pressed.contains(buttons[bit])) {
            flags = static_cast<std::uint8_t>(flags | (1U << bit));
        }
    }
    return {.action_flags = flags,
            .move_x = pack_axis(state, move_x),
            .move_y = pack_axis(state, move_y)};
}

engine::script::InputState unpack_input(const PackedInput& packed,
                                        std::span<const std::string_view> buttons) {
    engine::script::InputState state;
    state.axes.emplace(move_x, unpack_axis(packed.move_x));
    state.axes.emplace(move_y, unpack_axis(packed.move_y));
    for (std::size_t bit = 0; bit < usable(buttons); ++bit) {
        if ((packed.action_flags & (1U << bit)) != 0) {
            state.pressed.emplace(buttons[bit]);
        }
    }
    return state;
}

} // namespace rtype::common
