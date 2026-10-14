#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

#include <fcntl.h>

#include <gtest/gtest.h>

#include "UdpSocket.hpp"

using engine::network::Endpoint;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

namespace {

Endpoint endpoint_of(std::string_view ip, std::uint16_t port) {
    const auto endpoint = Endpoint::from_string(ip, port);
    EXPECT_TRUE(endpoint.has_value()) << ip;
    return endpoint.value_or(Endpoint{});
}

bool is_open_descriptor(int fd) { return ::fcntl(fd, F_GETFD) != -1; }

UdpSocket bound_loopback_socket() {
    UdpSocket socket(AF_INET);
    std::error_code ec;
    socket.bind(endpoint_of("127.0.0.1", 0), ec);
    EXPECT_FALSE(ec) << ec.message();
    return socket;
}

Endpoint local_endpoint_of(const UdpSocket& socket) {
    std::error_code ec;
    const auto endpoint = socket.local_endpoint(ec);
    EXPECT_FALSE(ec) << ec.message();
    return endpoint;
}

} // namespace

TEST(Endpoint, ParsesIpv4Address) {
    const auto endpoint = endpoint_of("127.0.0.1", 4242);

    EXPECT_EQ(endpoint.port(), 4242);
    EXPECT_EQ(endpoint.to_string(), "127.0.0.1:4242");
    EXPECT_EQ(endpoint.size(), sizeof(sockaddr_in));
}

TEST(Endpoint, ParsesIpv6Address) {
    const auto endpoint = endpoint_of("::1", 4242);

    EXPECT_EQ(endpoint.port(), 4242);
    EXPECT_EQ(endpoint.to_string(), "[::1]:4242");
    EXPECT_EQ(endpoint.size(), sizeof(sockaddr_in6));
}

TEST(Endpoint, RejectsMalformedAddresses) {
    EXPECT_FALSE(Endpoint::from_string("", 4242).has_value());
    EXPECT_FALSE(Endpoint::from_string("localhost", 4242).has_value());
    EXPECT_FALSE(Endpoint::from_string("256.0.0.1", 4242).has_value());
    EXPECT_FALSE(Endpoint::from_string("1.2.3", 4242).has_value());
    EXPECT_FALSE(Endpoint::from_string(std::string_view("127.0.0.1\0x", 11), 4242).has_value());
}

TEST(Endpoint, DefaultIsInvalid) {
    const Endpoint endpoint;

    EXPECT_EQ(endpoint.port(), 0);
    EXPECT_EQ(endpoint.to_string(), "<invalid>");
    EXPECT_EQ(endpoint.size(), 0U);
}

TEST(UdpSocket, DefaultConstructedIsClosed) {
    const UdpSocket socket;

    EXPECT_FALSE(socket.is_open());
    EXPECT_FALSE(static_cast<bool>(socket));
}

TEST(UdpSocket, CreateOpensSocket) {
    std::error_code ec;
    const auto socket = UdpSocket::create(AF_INET, ec);

    EXPECT_FALSE(ec);
    EXPECT_TRUE(socket.is_open());
}

TEST(UdpSocket, CreateReportsUnsupportedDomain) {
    std::error_code ec;
    const auto socket = UdpSocket::create(-1, ec);

    EXPECT_TRUE(ec);
    EXPECT_FALSE(socket.is_open());
}

TEST(UdpSocket, ConstructorThrowsOnUnsupportedDomain) {
    EXPECT_THROW(UdpSocket(-1), std::system_error);
}

TEST(UdpSocket, CloseIsIdempotent) {
    UdpSocket socket(AF_INET);

    socket.close();
    socket.close();

    EXPECT_FALSE(socket.is_open());
}

TEST(UdpSocket, OperationsOnClosedSocketReportBadDescriptor) {
    UdpSocket socket;
    const auto local = endpoint_of("127.0.0.1", 0);
    std::array<std::uint8_t, 4> buffer{};
    Endpoint from;
    std::error_code ec;
    const auto bad_descriptor = std::make_error_code(std::errc::bad_file_descriptor);

    socket.bind(local, ec);
    EXPECT_EQ(ec, bad_descriptor);
    socket.set_reuse_address(true, ec);
    EXPECT_EQ(ec, bad_descriptor);
    std::ignore = socket.local_endpoint(ec);
    EXPECT_EQ(ec, bad_descriptor);
    EXPECT_EQ(socket.send_to(buffer, local, ec), 0U);
    EXPECT_EQ(ec, bad_descriptor);
    EXPECT_EQ(socket.receive_from(buffer, from, 0ms, ec), 0U);
    EXPECT_EQ(ec, bad_descriptor);
}

TEST(UdpSocket, MoveConstructionTransfersOwnership) {
    int fd = -1;

    const UdpSocket target = [&fd] {
        UdpSocket source(AF_INET);
        fd = source.native_handle();
        return UdpSocket(std::move(source));
    }();

    EXPECT_EQ(target.native_handle(), fd);
    EXPECT_TRUE(is_open_descriptor(fd));
}

TEST(UdpSocket, MoveAssignmentTransfersOwnership) {
    UdpSocket target(AF_INET);
    const int replaced_fd = target.native_handle();
    int fd = -1;

    {
        UdpSocket source(AF_INET);
        fd = source.native_handle();
        target = std::move(source);
    }

    EXPECT_EQ(target.native_handle(), fd);
    EXPECT_TRUE(is_open_descriptor(fd));
    EXPECT_FALSE(is_open_descriptor(replaced_fd));
}

TEST(UdpSocket, BindRejectsAddressAlreadyInUse) {
    const auto first = bound_loopback_socket();
    UdpSocket second(AF_INET);
    std::error_code ec;

    second.bind(local_endpoint_of(first), ec);

    EXPECT_EQ(ec, std::make_error_code(std::errc::address_in_use));
}

TEST(UdpSocket, ReceiveTimesOutWhenNoDatagramIsQueued) {
    auto socket = bound_loopback_socket();
    std::array<std::uint8_t, 4> buffer{};
    Endpoint from;
    std::error_code ec;

    EXPECT_EQ(socket.receive_from(buffer, from, 0ms, ec), 0U);
    EXPECT_EQ(ec, std::make_error_code(std::errc::timed_out));
}

TEST(UdpSocket, SendsAndReceivesDatagramOnLoopback) {
    auto receiver = bound_loopback_socket();
    auto sender = bound_loopback_socket();
    const std::array<std::uint8_t, 3> payload{1, 2, 3};
    std::array<std::uint8_t, 16> buffer{};
    Endpoint from;
    std::error_code ec;

    EXPECT_EQ(sender.send_to(payload, local_endpoint_of(receiver), ec), payload.size());
    ASSERT_FALSE(ec) << ec.message();
    const auto received = receiver.receive_from(buffer, from, 1s, ec);

    ASSERT_FALSE(ec) << ec.message();
    ASSERT_EQ(received, payload.size());
    EXPECT_TRUE(std::equal(payload.begin(), payload.end(), buffer.begin()));
    EXPECT_EQ(from.to_string(), local_endpoint_of(sender).to_string());
}

TEST(UdpSocket, TruncatesDatagramLargerThanBuffer) {
    auto receiver = bound_loopback_socket();
    auto sender = bound_loopback_socket();
    const std::array<std::uint8_t, 8> payload{1, 2, 3, 4, 5, 6, 7, 8};
    std::array<std::uint8_t, 4> buffer{};
    Endpoint from;
    std::error_code ec;

    sender.send_to(payload, local_endpoint_of(receiver), ec);
    const auto received = receiver.receive_from(buffer, from, 1s, ec);

    ASSERT_FALSE(ec) << ec.message();
    EXPECT_EQ(received, buffer.size());
    EXPECT_EQ(buffer, (std::array<std::uint8_t, 4>{1, 2, 3, 4}));
}

TEST(UdpSocket, CloseUnblocksReceiverWaitingForever) {
    auto socket = bound_loopback_socket();
    std::error_code receive_error;

    std::thread receiver([&] {
        std::array<std::uint8_t, 4> buffer{};
        Endpoint from;
        socket.receive_from(buffer, from, -1ms, receive_error);
    });
    socket.close();
    receiver.join();

    EXPECT_TRUE(receive_error == std::errc::operation_canceled ||
                receive_error == std::errc::bad_file_descriptor)
        << receive_error.message();
}
