#include <cerrno>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "socket/SocketError.hpp"

namespace logger = engine::core::log;
using engine::network::socket_category;
using engine::network::socket_error_from_errno;
using engine::network::SocketError;

namespace {

std::vector<std::pair<int, SocketError>> mapped_errnos() {
    return {
        {EBADF, SocketError::closed},
        {ENOTSOCK, SocketError::closed},
        {EADDRINUSE, SocketError::address_in_use},
        {EADDRNOTAVAIL, SocketError::address_not_available},
        {EACCES, SocketError::permission_denied},
        {EPERM, SocketError::permission_denied},
        {ECONNREFUSED, SocketError::connection_refused},
        {EMSGSIZE, SocketError::message_too_large},
    };
}

std::vector<std::pair<SocketError, std::errc>> generic_conditions() {
    return {
        {SocketError::closed, std::errc::bad_file_descriptor},
        {SocketError::canceled, std::errc::operation_canceled},
        {SocketError::timed_out, std::errc::timed_out},
        {SocketError::address_in_use, std::errc::address_in_use},
        {SocketError::address_not_available, std::errc::address_not_available},
        {SocketError::permission_denied, std::errc::permission_denied},
        {SocketError::connection_refused, std::errc::connection_refused},
        {SocketError::message_too_large, std::errc::message_size},
    };
}

} // namespace

TEST(SocketError, ConvertsToErrorCodeInSocketCategory) {
    const std::error_code ec = SocketError::address_in_use;

    EXPECT_EQ(&ec.category(), &socket_category());
    EXPECT_STREQ(ec.category().name(), "socket");
    EXPECT_EQ(ec.message(), "address already in use");
}

TEST(SocketError, MapsKnownErrnoValues) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped(sink, logger::Level::Trace);

    for (const auto& [error, expected] : mapped_errnos()) {
        EXPECT_EQ(socket_error_from_errno(error), expected) << error;
    }
    EXPECT_TRUE(sink->records().empty());
}

TEST(SocketError, MapsUnknownErrnoToUnknownAndLogsOriginalValue) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped(sink, logger::Level::Trace);

    EXPECT_EQ(socket_error_from_errno(ENOMEM), SocketError::unknown);

    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, logger::Level::Warn);
    EXPECT_NE(records.front().message.find(std::to_string(ENOMEM)), std::string::npos)
        << records.front().message;
}

TEST(SocketError, MatchesEquivalentGenericConditions) {
    for (const auto& [error, condition] : generic_conditions()) {
        EXPECT_EQ(make_error_code(error), condition) << make_error_code(error).message();
    }
}

TEST(SocketError, UnknownMatchesNoGenericCondition) {
    const std::error_code ec = SocketError::unknown;

    EXPECT_NE(ec, std::errc::io_error);
    EXPECT_EQ(ec.default_error_condition().category(), socket_category());
}

TEST(SocketError, DescribesUnrecognizedValue) {
    EXPECT_EQ(socket_category().message(0), "unrecognized socket error");
}
