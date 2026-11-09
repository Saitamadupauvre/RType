#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "engine/network/Export.hpp"

#include "socket/UdpSocket.hpp"
#include "thread/NetworkIo.hpp"
#include "transport/Link.hpp"

namespace engine::network {

inline constexpr std::size_t max_known_peers = 1024;

class ENGINE_NETWORK_EXPORT UdpServerLink final : public Link {
public:
    explicit UdpServerLink(std::uint16_t port, std::size_t max_peers = max_known_peers);

    void open(std::error_code& ec) override;
    void close() override;
    [[nodiscard]] std::vector<Inbound> poll() override;
    void send(Outbound outbound) override;
    void forget(PeerId peer) override;
    [[nodiscard]] std::string describe(PeerId peer) const override;

    [[nodiscard]] std::uint16_t port() const;

private:
    struct KnownPeer {
        PeerId peer;
        Endpoint endpoint;
    };

    [[nodiscard]] std::optional<PeerId> peer_of(const Endpoint& endpoint);

    std::uint16_t _port;
    std::size_t _max_peers;
    std::vector<KnownPeer> _peers;
    std::uint32_t _next_peer{1};
    NetworkIo _io;
};

class ENGINE_NETWORK_EXPORT UdpClientLink final : public Link {
public:
    UdpClientLink(std::string server_ip, std::uint16_t port);

    void open(std::error_code& ec) override;
    void close() override;
    [[nodiscard]] std::vector<Inbound> poll() override;
    void send(Outbound outbound) override;
    void forget(PeerId peer) override;
    [[nodiscard]] std::string describe(PeerId peer) const override;

private:
    std::string _server_ip;
    std::uint16_t _port;
    std::optional<Endpoint> _server;
    NetworkIo _io;
};

} // namespace engine::network
