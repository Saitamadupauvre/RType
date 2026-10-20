#pragma once

#include <cstdint>

namespace engine::network {

[[nodiscard]] constexpr bool is_newer_sequence(std::uint32_t candidate,
                                               std::uint32_t latest) noexcept {
    return static_cast<std::int32_t>(candidate - latest) > 0;
}

} // namespace engine::network
