#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

#include "engine/network/Export.hpp"

#include "protocol/Protocol.hpp"
#include "protocol/ProtocolHandler.hpp"
#include "thread/NetworkIo.hpp"
#include "transport/Link.hpp"

namespace engine::network {

class LoopbackHub;

class ENGINE_NETWORK_EXPORT LoopbackLink final : public Link {
public:
    LoopbackLink(LoopbackHub& hub, PeerId self);

    void open(std::error_code& ec) override;
    void close() override;
    [[nodiscard]] std::vector<Inbound> poll() override;
    void send(Outbound outbound) override;
    void forget(PeerId peer) override;
    [[nodiscard]] std::string describe(PeerId peer) const override;

    [[nodiscard]] PeerId self() const noexcept { return _self; }

private:
    friend class LoopbackHub;

    struct Packet {
        PeerId from;
        Bytes bytes;
    };

    LoopbackHub& _hub;
    PeerId _self;
    bool _open{false};
    bool _cut{false};
    std::vector<Packet> _inbox;
};

class ENGINE_NETWORK_EXPORT LoopbackHub {
public:
    explicit LoopbackHub(std::size_t inbox_capacity = default_inbox_capacity);

    [[nodiscard]] LoopbackLink& server() noexcept { return *_server; }
    [[nodiscard]] LoopbackLink& add_client();
    void cut(PeerId client);

private:
    friend class LoopbackLink;

    [[nodiscard]] LoopbackLink* find(PeerId peer);
    void deliver(PeerId from, PeerId to, Bytes bytes);

    std::size_t _inbox_capacity;
    ProtocolHandler _protocol;
    std::mutex _mutex;
    std::unique_ptr<LoopbackLink> _server;
    std::vector<std::unique_ptr<LoopbackLink>> _clients;
};

[[nodiscard]] ENGINE_NETWORK_EXPORT std::string loopback_name(PeerId peer);

} // namespace engine::network
