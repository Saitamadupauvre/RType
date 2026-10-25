#include "SocketError.hpp"

#include <cerrno>
#include <string>

#include "engine/core/log/Log.hpp"

namespace engine::network {

namespace {

class SocketCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "socket"; }

    [[nodiscard]] std::string message(int value) const override {
        switch (static_cast<SocketError>(value)) {
        case SocketError::closed:
            return "socket is closed";
        case SocketError::canceled:
            return "operation canceled by close";
        case SocketError::timed_out:
            return "operation timed out";
        case SocketError::address_in_use:
            return "address already in use";
        case SocketError::address_not_available:
            return "address not available";
        case SocketError::permission_denied:
            return "permission denied";
        case SocketError::connection_refused:
            return "connection refused";
        case SocketError::message_too_large:
            return "message too large";
        case SocketError::unknown:
            return "unknown socket error";
        }
        return "unrecognized socket error";
    }

    [[nodiscard]] std::error_condition default_error_condition(int value) const noexcept override {
        switch (static_cast<SocketError>(value)) {
        case SocketError::closed:
            return std::errc::bad_file_descriptor;
        case SocketError::canceled:
            return std::errc::operation_canceled;
        case SocketError::timed_out:
            return std::errc::timed_out;
        case SocketError::address_in_use:
            return std::errc::address_in_use;
        case SocketError::address_not_available:
            return std::errc::address_not_available;
        case SocketError::permission_denied:
            return std::errc::permission_denied;
        case SocketError::connection_refused:
            return std::errc::connection_refused;
        case SocketError::message_too_large:
            return std::errc::message_size;
        case SocketError::unknown:
            break;
        }
        return {value, *this};
    }
};

SocketError socket_error_of(int error) noexcept {
    switch (error) {
    case EBADF:
    case ENOTSOCK:
        return SocketError::closed;
    case EADDRINUSE:
        return SocketError::address_in_use;
    case EADDRNOTAVAIL:
        return SocketError::address_not_available;
    case EACCES:
    case EPERM:
        return SocketError::permission_denied;
    case ECONNREFUSED:
        return SocketError::connection_refused;
    case EMSGSIZE:
        return SocketError::message_too_large;
    default:
        return SocketError::unknown;
    }
}

} // namespace

const std::error_category& socket_category() noexcept {
    static const SocketCategory category;
    return category;
}

std::error_code socket_error_from_errno(int error) noexcept {
    const auto mapped = socket_error_of(error);
    if (mapped == SocketError::unknown) {
        core::log::warn("network: unmapped socket error {} ({})", error,
                        std::generic_category().message(error));
    }
    return make_error_code(mapped);
}

} // namespace engine::network
