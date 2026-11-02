#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "protocol/Messages.hpp"
#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "socket/UdpSocket.hpp"
#include "transport/UdpLinks.hpp"

namespace logger = engine::core::log;

namespace {

using engine::network::ConnectRequestPayload;
using engine::network::Endpoint;
using engine::network::HeartbeatPayload;
using engine::network::Inbound;
using engine::network::Link;
using engine::network::max_datagram_size;
using engine::network::Message;
using engine::network::Outbound;
using engine::network::PeerId;
using engine::network::ProtocolHandler;
using engine::network::ReceivedMessage;
using engine::network::server_peer;
using engine::network::UdpClientLink;
using engine::network::UdpServerLink;
using engine::network::UdpSocket;
using namespace std::chrono_literals;

constexpr auto delivery_deadline = 2s;

Endpoint loopback(std::uint16_t port) {
    const auto endpoint = Endpoint::from_string("127.0.0.1", port);
    EXPECT_TRUE(endpoint.has_value());
    return endpoint.value_or(Endpoint{});
}

UdpSocket bound_peer() {
    UdpSocket socket(AF_INET);
    std::error_code ec;
    socket.bind(loopback(0), ec);
    EXPECT_FALSE(ec) << ec.message();
    return socket;
}

std::uint16_t port_of(const UdpSocket& socket) {
    std::error_code ec;
    return socket.local_endpoint(ec).port();
}

Message request(std::uint32_t nonce) { return ConnectRequestPayload{.client_nonce = nonce}; }

void send_raw(UdpSocket& peer, const Message& message, std::uint16_t port) {
    const auto bytes = ProtocolHandler{}.encode(message, 0);
    ASSERT_TRUE(bytes.has_value());
    std::error_code ec;
    peer.send_to(*bytes, loopback(port), ec);
    ASSERT_FALSE(ec) << ec.message();
}

std::optional<ReceivedMessage> receive_raw(UdpSocket& peer, Endpoint& from) {
    std::array<std::uint8_t, max_datagram_size> buffer{};
    std::error_code ec;
    const auto size = peer.receive_from(buffer, from, delivery_deadline, ec);
    if (ec) {
        return std::nullopt;
    }
    return ProtocolHandler{}.decode(std::span(buffer).first(size));
}

std::vector<Inbound> poll_until(Link& link, std::size_t count) {
    const auto deadline = std::chrono::steady_clock::now() + delivery_deadline;
    std::vector<Inbound> received;
    while (received.size() < count && std::chrono::steady_clock::now() < deadline) {
        for (auto& inbound : link.poll()) {
            received.push_back(std::move(inbound));
        }
        std::this_thread::yield();
    }
    return received;
}

void open(Link& link) {
    std::error_code ec;
    link.open(ec);
    ASSERT_FALSE(ec) << ec.message();
}

class UdpServerLinkTest : public testing::Test {
protected:
    void SetUp() override { open(link); }

    UdpServerLink link{0, 2};
    UdpSocket first = bound_peer();
    UdpSocket second = bound_peer();
    std::shared_ptr<logger::MemorySink> sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped{sink, logger::Level::Trace};
};

} // namespace

TEST_F(UdpServerLinkTest, GivesOnePeerToEachAddress) {
    send_raw(first, request(1), link.port());
    send_raw(second, request(2), link.port());
    send_raw(first, request(3), link.port());

    const auto inbound = poll_until(link, 3);

    ASSERT_EQ(inbound.size(), 3U);
    EXPECT_EQ(inbound[0].from, inbound[2].from);
    EXPECT_NE(inbound[0].from, inbound[1].from);
    EXPECT_EQ(link.describe(inbound[0].from), "127.0.0.1:" + std::to_string(port_of(first)));
}

TEST_F(UdpServerLinkTest, SendsToThePeerAddress) {
    send_raw(first, request(1), link.port());
    const auto inbound = poll_until(link, 1);
    ASSERT_EQ(inbound.size(), 1U);

    link.send(Outbound{.to = inbound.front().from, .message = HeartbeatPayload{}, .sequence = 3});

    Endpoint from;
    const auto received = receive_raw(first, from);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->header.sequence, 3U);
    EXPECT_EQ(received->message, Message{HeartbeatPayload{}});
}

TEST_F(UdpServerLinkTest, ForgottenPeersAreUnknownAndComeBackWithANewId) {
    send_raw(first, request(1), link.port());
    const auto before = poll_until(link, 1);
    ASSERT_EQ(before.size(), 1U);
    const auto old_peer = before.front().from;

    link.forget(old_peer);
    link.send(Outbound{.to = old_peer, .message = HeartbeatPayload{}, .sequence = 0});
    send_raw(first, request(2), link.port());
    const auto after = poll_until(link, 1);

    EXPECT_EQ(link.describe(old_peer).find("unknown peer"), 0U);
    ASSERT_EQ(after.size(), 1U);
    EXPECT_NE(after.front().from, old_peer);
}

TEST_F(UdpServerLinkTest, DropsAndLogsNewAddressesBeyondTheLimit) {
    auto third = bound_peer();
    send_raw(first, request(1), link.port());
    send_raw(second, request(2), link.port());
    ASSERT_EQ(poll_until(link, 2).size(), 2U);

    send_raw(third, request(3), link.port());
    send_raw(first, request(4), link.port());
    const auto inbound = poll_until(link, 1);

    ASSERT_EQ(inbound.size(), 1U);
    EXPECT_EQ(inbound.front().message.message, request(4));
    EXPECT_TRUE(std::ranges::any_of(sink->records(), [](const logger::MemoryRecord& record) {
        return record.message.find("too many peers") != std::string::npos;
    }));
}

TEST_F(UdpServerLinkTest, CloseForgetsEveryPeer) {
    send_raw(first, request(1), link.port());
    const auto inbound = poll_until(link, 1);
    ASSERT_EQ(inbound.size(), 1U);

    link.close();

    EXPECT_EQ(link.describe(inbound.front().from).find("unknown peer"), 0U);
}

TEST(UdpClientLink, RejectsANonNumericAddress) {
    UdpClientLink link("not an address", 4242);
    std::error_code ec;

    link.open(ec);

    EXPECT_EQ(ec, std::errc::invalid_argument);
    EXPECT_EQ(link.describe(server_peer), "not an address:4242");
}

TEST(UdpClientLink, ExchangesWithTheServerOnlyAsTheServerPeer) {
    auto server = bound_peer();
    auto stranger = bound_peer();
    UdpClientLink link("127.0.0.1", port_of(server));
    open(link);
    EXPECT_EQ(link.describe(server_peer), "127.0.0.1:" + std::to_string(port_of(server)));

    link.send(Outbound{.to = PeerId{7}, .message = request(6), .sequence = 0});
    link.send(Outbound{.to = server_peer, .message = request(5), .sequence = 0});
    Endpoint client;
    const auto sent = receive_raw(server, client);
    ASSERT_TRUE(sent.has_value());
    EXPECT_EQ(sent->message, request(5));

    send_raw(stranger, request(8), client.port());
    send_raw(server, request(9), client.port());
    const auto inbound = poll_until(link, 1);

    ASSERT_EQ(inbound.size(), 1U);
    EXPECT_EQ(inbound.front().from, server_peer);
    EXPECT_EQ(inbound.front().message.message, request(9));
}
