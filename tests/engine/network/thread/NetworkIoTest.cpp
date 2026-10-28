#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "PayloadSamples.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/SocketError.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkIo.hpp"

namespace logger = engine::core::log;

namespace {

using engine::network::AddressedInbound;
using engine::network::AddressedOutbound;
using engine::network::Bytes;
using engine::network::connect_accept_prefix_size;
using engine::network::Endpoint;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::max_datagram_size;
using engine::network::max_payload_size;
using engine::network::max_string_name_length;
using engine::network::Message;
using engine::network::min_string_entry_size;
using engine::network::NetworkIo;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::SocketError;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

constexpr auto delivery_deadline = 2s;

Endpoint loopback(std::uint16_t port = 0) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

Message sample_message() {
    return InputPayload{.keys = {0x02}, .mouse_buttons = 0x01, .pointer = std::nullopt};
}

Bytes encoded(const Message& message) {
    const ProtocolHandler handler;
    auto bytes = handler.encode(message, 0);
    EXPECT_TRUE(bytes.has_value());
    return std::move(bytes).value_or(Bytes{});
}

UdpSocket bound_peer() {
    UdpSocket socket(AF_INET);
    std::error_code ec;
    socket.bind(loopback(), ec);
    EXPECT_FALSE(ec) << ec.message();
    return socket;
}

Endpoint local_endpoint_of(const UdpSocket& socket) {
    std::error_code ec;
    const auto endpoint = socket.local_endpoint(ec);
    EXPECT_FALSE(ec) << ec.message();
    return endpoint;
}

Endpoint local_endpoint_of(const NetworkIo& io) {
    std::error_code ec;
    const auto endpoint = io.local_endpoint(ec);
    EXPECT_FALSE(ec) << ec.message();
    return endpoint;
}

void start_on_loopback(NetworkIo& io) {
    std::error_code ec;
    io.start(loopback(), ec);
    ASSERT_FALSE(ec) << ec.message();
}

void send_raw(UdpSocket& peer, const Bytes& datagram, const Endpoint& to) {
    std::error_code ec;
    peer.send_to(datagram, to, ec);
    ASSERT_FALSE(ec) << ec.message();
}

std::vector<AddressedInbound> poll_until_received(NetworkIo& io) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    std::vector<AddressedInbound> received = io.poll();
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        received = io.poll();
    }
    return received;
}

bool wait_for_log_containing(const logger::MemorySink& sink, std::string_view text) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    const auto contains_text = [text](const logger::MemoryRecord& record) {
        return record.message.find(text) != std::string::npos;
    };
    while (!std::ranges::any_of(sink.records(), contains_text)) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::yield();
    }
    return true;
}

Bytes max_size_datagram() {
    constexpr std::size_t full_entries = 17;
    constexpr std::size_t full_entry_size = min_string_entry_size + max_string_name_length;
    constexpr std::size_t last_name_length = max_payload_size - connect_accept_prefix_size -
                                             (full_entries * full_entry_size) -
                                             min_string_entry_size;
    auto strings = payload_samples::long_name_strings(full_entries);
    EXPECT_TRUE(strings.insert({.category = engine::network::StringCategory::type,
                                .id = full_entries,
                                .name = std::string(last_name_length, 'z')}));

    auto datagram = encoded(payload_samples::accept_with(std::move(strings)));
    EXPECT_EQ(datagram.size(), max_datagram_size);
    return datagram;
}

std::optional<ReceivedMessage> receive_decoded(UdpSocket& peer) {
    std::array<std::uint8_t, max_datagram_size> buffer{};
    Endpoint from;
    std::error_code ec;
    const auto size = peer.receive_from(buffer, from, delivery_deadline, ec);
    EXPECT_FALSE(ec) << ec.message();
    if (ec) {
        return std::nullopt;
    }
    const ProtocolHandler handler;
    return handler.decode(std::span(buffer).first(size));
}

} // namespace

TEST(NetworkIo, StartsAndStopsOnLoopbackWithoutHanging) {
    NetworkIo never_started;
    never_started.stop();

    NetworkIo io;
    start_on_loopback(io);

    EXPECT_NE(local_endpoint_of(io).port(), 0);
    io.stop();
    io.stop();
}

TEST(NetworkIo, StartReportsAddressAlreadyInUse) {
    const auto occupant = bound_peer();
    NetworkIo io;
    std::error_code ec;

    io.start(local_endpoint_of(occupant), ec);

    EXPECT_EQ(ec, SocketError::address_in_use);
    std::ignore = io.local_endpoint(ec);
    EXPECT_EQ(ec, SocketError::closed);
}

TEST(NetworkIo, StartTwiceReportsAlreadyStarted) {
    NetworkIo io;
    start_on_loopback(io);
    const auto first = local_endpoint_of(io);
    std::error_code ec;

    io.start(loopback(), ec);

    EXPECT_EQ(ec, std::errc::already_connected);
    EXPECT_EQ(local_endpoint_of(io).to_string(), first.to_string());
}

TEST(NetworkIo, RestartsAfterStop) {
    NetworkIo io;
    start_on_loopback(io);
    io.stop();
    start_on_loopback(io);
    auto peer = bound_peer();

    send_raw(peer, encoded(sample_message()), local_endpoint_of(io));
    io.send(AddressedOutbound{.to = local_endpoint_of(peer), .message = sample_message()});

    EXPECT_EQ(poll_until_received(io).size(), 1U);
    const auto received = receive_decoded(peer);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->message, sample_message());
}

TEST(NetworkIo, PollReturnsDatagramSentByPeer) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();

    send_raw(peer, encoded(sample_message()), local_endpoint_of(io));
    const auto received = poll_until_received(io);

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().from.to_string(), local_endpoint_of(peer).to_string());
    EXPECT_EQ(received.front().message.message, sample_message());
}

TEST(NetworkIo, PollNeverReturnsGarbageDatagram) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();
    const auto to = local_endpoint_of(io);

    send_raw(peer, Bytes{0xDE, 0xAD, 0xBE, 0xEF}, to);
    send_raw(peer, encoded(HeartbeatPayload{}), to);
    const auto received = poll_until_received(io);

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().message.message, Message{HeartbeatPayload{}});
}

TEST(NetworkIo, AcceptsDatagramOfMaximumSize) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();

    send_raw(peer, max_size_datagram(), local_endpoint_of(io));

    EXPECT_EQ(poll_until_received(io).size(), 1U);
}

TEST(NetworkIo, DropsAndLogsOversizedDatagramInsteadOfTruncatingIt) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped(sink, logger::Level::Trace);
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();
    const auto to = local_endpoint_of(io);
    auto oversized = max_size_datagram();
    oversized.push_back(0);

    send_raw(peer, oversized, to);
    send_raw(peer, encoded(HeartbeatPayload{}), to);
    const auto received = poll_until_received(io);

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().message.message, Message{HeartbeatPayload{}});
    EXPECT_TRUE(wait_for_log_containing(*sink, "oversized"));
}

TEST(NetworkIo, FullInboxDropsAndLogsExtraMessages) {
    const auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scoped(sink, logger::Level::Trace);
    constexpr std::size_t capacity = 2;
    NetworkIo io(capacity);
    start_on_loopback(io);
    auto peer = bound_peer();
    const auto to = local_endpoint_of(io);

    for (std::size_t i = 0; i <= capacity; ++i) {
        send_raw(peer, encoded(HeartbeatPayload{}), to);
    }

    ASSERT_TRUE(wait_for_log_containing(*sink, "inbox full"));
    EXPECT_EQ(io.poll().size(), capacity);
}

TEST(NetworkIo, SendReachesPeerAndDecodesToSameMessage) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();

    io.send(AddressedOutbound{
        .to = local_endpoint_of(peer), .message = sample_message(), .sequence = 41});
    const auto received = receive_decoded(peer);

    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->message, sample_message());
    EXPECT_EQ(received->header.sequence, 41U);
}

TEST(NetworkIo, StopFlushesQueuedSendsBeforeJoining) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();
    const auto to = local_endpoint_of(peer);
    constexpr std::uint32_t queued = 3;

    for (std::uint32_t sequence = 0; sequence < queued; ++sequence) {
        io.send(AddressedOutbound{.to = to, .message = sample_message(), .sequence = sequence});
    }
    io.stop();

    for (std::uint32_t sequence = 0; sequence < queued; ++sequence) {
        const auto received = receive_decoded(peer);
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(received->header.sequence, sequence);
        EXPECT_EQ(received->message, sample_message());
    }
}

TEST(NetworkIo, SendIsDeliveredWhileNoDatagramArrives) {
    NetworkIo io;
    start_on_loopback(io);
    auto peer = bound_peer();
    const auto to = local_endpoint_of(peer);

    for (std::uint32_t sequence = 0; sequence < 3; ++sequence) {
        io.send(AddressedOutbound{.to = to, .message = HeartbeatPayload{}, .sequence = sequence});
        const auto received = receive_decoded(peer);
        ASSERT_TRUE(received.has_value());
        EXPECT_EQ(received->header.sequence, sequence);
    }
    EXPECT_TRUE(io.poll().empty());
}
