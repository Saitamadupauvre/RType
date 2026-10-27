#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"

#include "LoopbackPeers.hpp"
#include "protocol/Messages.hpp"
#include "transport/LoopbackLinks.hpp"

namespace logger = engine::core::log;

namespace {

using engine::network::ConnectRequestPayload;
using engine::network::HeartbeatPayload;
using engine::network::InputPayload;
using engine::network::LoopbackHub;
using engine::network::LoopbackLink;
using engine::network::Message;
using engine::network::PeerId;
using engine::network::server_peer;
using loopback_peers::messages;
using loopback_peers::open;
using loopback_peers::send;

Message request(std::uint32_t nonce) { return ConnectRequestPayload{.client_nonce = nonce}; }

class LoopbackLinks : public testing::Test {
protected:
    LoopbackHub hub{4};
    LoopbackLink& server = open(hub.server());
    LoopbackLink& first = open(hub.add_client());
    LoopbackLink& second = open(hub.add_client());
    std::shared_ptr<logger::MemorySink> sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink scoped{sink, logger::Level::Trace};
};

} // namespace

TEST_F(LoopbackLinks, NumbersClientsFromOneAndTheServerZero) {
    EXPECT_EQ(server.self(), server_peer);
    EXPECT_EQ(first.self(), PeerId{1});
    EXPECT_EQ(second.self(), PeerId{2});
}

TEST_F(LoopbackLinks, DeliversToTheServerInOrderWithTheSender) {
    send(first, server_peer, request(1), 4);
    send(second, server_peer, request(2));
    send(first, server_peer, request(3));

    const auto inbound = server.poll();

    ASSERT_EQ(inbound.size(), 3U);
    EXPECT_EQ(inbound[0].from, PeerId{1});
    EXPECT_EQ(inbound[0].message.header.sequence, 4U);
    EXPECT_EQ(inbound[0].message.message, request(1));
    EXPECT_EQ(inbound[1].from, PeerId{2});
    EXPECT_EQ(inbound[2].message.message, request(3));
    EXPECT_TRUE(server.poll().empty());
}

TEST_F(LoopbackLinks, DeliversToTheAddressedClientOnly) {
    send(server, PeerId{2}, HeartbeatPayload{});

    EXPECT_TRUE(messages(first).empty());
    const auto inbound = second.poll();
    ASSERT_EQ(inbound.size(), 1U);
    EXPECT_EQ(inbound.front().from, server_peer);
}

TEST_F(LoopbackLinks, DropsPacketsToAnUnknownPeer) {
    send(server, PeerId{9}, HeartbeatPayload{});
    send(first, PeerId{9}, HeartbeatPayload{});

    EXPECT_TRUE(messages(first).empty());
    EXPECT_TRUE(messages(second).empty());
    EXPECT_TRUE(messages(server).empty());
}

TEST_F(LoopbackLinks, DropsPacketsToAndFromAClosedLink) {
    send(first, server_peer, request(1));
    server.close();

    send(first, server_peer, request(2));
    EXPECT_TRUE(messages(server).empty());

    open(server);
    first.close();
    send(first, server_peer, request(3));
    send(server, PeerId{1}, HeartbeatPayload{});
    EXPECT_TRUE(messages(server).empty());
    open(first);
    EXPECT_TRUE(messages(first).empty());
}

TEST_F(LoopbackLinks, CutDropsEverythingToAndFromTheClient) {
    hub.cut(PeerId{1});

    send(first, server_peer, request(1));
    send(server, PeerId{1}, HeartbeatPayload{});
    send(second, server_peer, request(2));

    EXPECT_TRUE(messages(first).empty());
    EXPECT_EQ(messages(server), (std::vector<Message>{request(2)}));
}

TEST_F(LoopbackLinks, CuttingTheServerOrAnUnknownPeerDoesNothing) {
    hub.cut(server_peer);
    hub.cut(PeerId{9});

    send(first, server_peer, request(1));

    EXPECT_EQ(messages(server).size(), 1U);
}

TEST_F(LoopbackLinks, DropsAndLogsPacketsBeyondTheInboxCapacity) {
    for (std::uint32_t nonce = 0; nonce < 6; ++nonce) {
        send(first, server_peer, request(nonce));
    }

    EXPECT_EQ(messages(server).size(), 4U);
    const auto records = sink->records();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_NE(records.front().message.find("loopback client 1"), std::string::npos);
}

TEST_F(LoopbackLinks, DropsAndLogsAMessageThatCannotBeEncoded) {
    send(server, PeerId{1},
         InputPayload{.keys = {0x01}, .mouse_buttons = 0, .pointer = std::nullopt});

    EXPECT_TRUE(messages(first).empty());
    const auto records = sink->records();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records.front().level, logger::Level::Warn);
    EXPECT_NE(records.front().message.find("loopback client 1"), std::string::npos);
}

TEST(LoopbackLinkNames, NameTheServerAndEachClient) {
    LoopbackHub hub;

    EXPECT_EQ(hub.server().describe(server_peer), "loopback server");
    EXPECT_EQ(hub.server().describe(PeerId{3}), "loopback client 3");
}
