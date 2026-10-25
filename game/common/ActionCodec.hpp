#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "engine/script/InputState.hpp"

namespace rtype::common {

struct PackedInput {
    std::uint8_t action_flags;
    std::int8_t move_x;
    std::int8_t move_y;

    bool operator==(const PackedInput&) const = default;
};

inline constexpr std::array<std::string_view, 1> action_buttons{"fire"};

PackedInput pack_input(const engine::script::InputState& state,
                       std::span<const std::string_view> buttons = action_buttons);

engine::script::InputState unpack_input(const PackedInput& packed,
                                        std::span<const std::string_view> buttons = action_buttons);

} // namespace rtype::common
