#pragma once

#include <cstddef>
#include <cstdint>

namespace engine::core::event {

/**
 * @brief Handle identifying one handler registered on an EventBus.
 *
 * @note Copyable value type. Passing a stale or default handle to EventBus::unsubscribe is
 * harmless.
 */
struct Subscription {
    std::size_t type{0};
    std::uint64_t id{0};
};

} // namespace engine::core::event
