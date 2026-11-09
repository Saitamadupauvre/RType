#include "UdpLinks.hpp"

#include <algorithm>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::network {

namespace {

Endpoint any_local_endpoint(const Endpoint& server) {
    const auto any_address = server.addr()->sa_family == AF_INET6 ? "::" : "0.0.0.0";
    return Endpoint::from_string(any_address, 0).value_or(Endpoint{});
}

std::string unknown_peer(PeerId peer) {
    return "unknown peer " + std::to_string(static_cast<std::uint32_t>(peer));
}

} // namespace

UdpServerLink::UdpServerLink(std::uint16_t port, std::size_t max_peers)
    : _port(port), _max_peers(max_peers) {}

void UdpServerLink::open(std::error_code& ec) {
    const auto any_address = Endpoint::from_string("0.0.0.0", _port);
    _io.start(any_address.value_or(Endpoint{}), ec);
}

void UdpServerLink::close() {
    _io.stop();
    _peers.clear();
}

std::vector<Inbound> UdpServerLink::poll() {
    std::vector<Inbound> inbound;
    for (auto& received : _io.poll()) {
        if (const auto peer = peer_of(received.from)) {
            inbound.push_back(Inbound{.from = *peer, .message = std::move(received.message)});
        }
    }
    return inbound;
}

void UdpServerLink::send(Outbound outbound) {
    const auto known = std::ranges::find(_peers, outbound.to, &KnownPeer::peer);
    if (known == _peers.end()) {
        core::log::debug("network: dropped message to {}", unknown_peer(outbound.to));
        return;
    }
    _io.send(AddressedOutbound{.to = known->endpoint,
                               .message = std::move(outbound.message),
                               .sequence = outbound.sequence});
}

void UdpServerLink::forget(PeerId peer) {
    std::erase_if(_peers, [peer](const KnownPeer& known) { return known.peer == peer; });
}

std::string UdpServerLink::describe(PeerId peer) const {
    const auto known = std::ranges::find(_peers, peer, &KnownPeer::peer);
    return known == _peers.end() ? unknown_peer(peer) : known->endpoint.to_string();
}

std::uint16_t UdpServerLink::port() const {
    std::error_code ec;
    const auto local = _io.local_endpoint(ec);
    return ec ? _port : local.port();
}

std::optional<PeerId> UdpServerLink::peer_of(const Endpoint& endpoint) {
    const auto known = std::ranges::find(_peers, endpoint, &KnownPeer::endpoint);
    if (known != _peers.end()) {
        return known->peer;
    }
    if (_peers.size() >= _max_peers) {
        core::log::warn("network: dropped message from {}: too many peers", endpoint.to_string());
        return std::nullopt;
    }
    const PeerId peer{_next_peer++};
    _peers.push_back(KnownPeer{.peer = peer, .endpoint = endpoint});
    return peer;
}

UdpClientLink::UdpClientLink(std::string server_ip, std::uint16_t port)
    : _server_ip(std::move(server_ip)), _port(port) {}

void UdpClientLink::open(std::error_code& ec) {
    ec.clear();
    const auto server = Endpoint::from_string(_server_ip, _port);
    if (!server) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return;
    }
    _io.start(any_local_endpoint(*server), ec);
    if (!ec) {
        _server = server;
    }
}

void UdpClientLink::close() { _io.stop(); }

std::vector<Inbound> UdpClientLink::poll() {
    std::vector<Inbound> inbound;
    for (auto& received : _io.poll()) {
        if (received.from != _server) {
            core::log::debug("network: ignored message from {}, not the server",
                             received.from.to_string());
            continue;
        }
        inbound.push_back(Inbound{.from = server_peer, .message = std::move(received.message)});
    }
    return inbound;
}

void UdpClientLink::send(Outbound outbound) {
    if (!_server || outbound.to != server_peer) {
        core::log::debug("network: dropped message to {}", unknown_peer(outbound.to));
        return;
    }
    _io.send(AddressedOutbound{
        .to = *_server, .message = std::move(outbound.message), .sequence = outbound.sequence});
}

void UdpClientLink::forget(PeerId) {}

std::string UdpClientLink::describe(PeerId peer) const {
    if (peer != server_peer) {
        return unknown_peer(peer);
    }
    return _server ? _server->to_string() : _server_ip + ":" + std::to_string(_port);
}

} // namespace engine::network
