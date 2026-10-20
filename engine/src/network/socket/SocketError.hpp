#pragma once

#include <system_error>
#include <type_traits>

#include "engine/network/Export.hpp"

namespace engine::network {

enum class SocketError {
    closed = 1,
    canceled,
    timed_out,
    address_in_use,
    address_not_available,
    permission_denied,
    connection_refused,
    message_too_large,
    unknown,
};

[[nodiscard]] ENGINE_NETWORK_EXPORT const std::error_category& socket_category() noexcept;

[[nodiscard]] inline std::error_code make_error_code(SocketError error) noexcept {
    return {static_cast<int>(error), socket_category()};
}

[[nodiscard]] ENGINE_NETWORK_EXPORT std::error_code socket_error_from_errno(int error) noexcept;

} // namespace engine::network

template <> struct std::is_error_code_enum<engine::network::SocketError> : std::true_type {};
