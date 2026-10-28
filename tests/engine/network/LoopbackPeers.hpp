#pragma once

#include <cstdint>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "engine/network/Transport.hpp"

#include "protocol/Messages.hpp"
#include "thread/NetworkMessages.hpp"
#include "transport/Link.hpp"
#include "transport/LoopbackLinks.hpp"

namespace loopback_peers {

class HubServerTransport final : public engine::network::ServerTransport {
public:
    explicit HubServerTransport(engine::network::LoopbackLink& link) : _link(link) {}

private:
    [[nodiscard]] engine::network::Link& link() noexcept override { return _link; }

    engine::network::LoopbackLink& _link;
};

class HubClientTransport final : public engine::network::ClientTransport {
public:
    explicit HubClientTransport(engine::network::LoopbackLink& link) : _link(link) {}

private:
    [[nodiscard]] engine::network::Link& link() noexcept override { return _link; }

    engine::network::LoopbackLink& _link;
};

inline engine::network::LoopbackLink& open(engine::network::LoopbackLink& link) {
    std::error_code ec;
    link.open(ec);
    EXPECT_FALSE(ec) << ec.message();
    return link;
}

inline void send(engine::network::LoopbackLink& link, engine::network::PeerId to,
                 engine::network::Message message, std::uint32_t sequence = 0) {
    link.send(
        engine::network::Outbound{.to = to, .message = std::move(message), .sequence = sequence});
}

inline std::vector<engine::network::Message> messages(engine::network::LoopbackLink& link) {
    std::vector<engine::network::Message> received;
    for (auto& inbound : link.poll()) {
        received.push_back(std::move(inbound.message.message));
    }
    return received;
}

} // namespace loopback_peers
